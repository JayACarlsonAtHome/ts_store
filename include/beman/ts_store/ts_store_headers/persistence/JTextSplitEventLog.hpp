#pragma once

// JTextSplitEventLog (improved minimal header-only version)
// 
// Goals for this iteration (low pain, high value):
// - Open 3 files early + write headers/Field Lists before any events
// - Use 10K auto-batching on all writers (max throughput as requested)
// - Write proper split data: main + ints (with linking ID) + floats (with linking ID)
// - Support PersistMode::KeeperOnly
// - Collect basic metrics (rows written, batches flushed)
// - Stay header-only to avoid .cpp redefinition / guard pain

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include <chrono>

#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
#include "jText.h"
#endif

#include "PersistCommon.hpp"

namespace jac::ts_store::inline_v001 {

struct JTextSplitEventLogStats {
    size_t main_rows = 0;
    size_t ints_rows = 0;
    size_t floats_rows = 0;
    size_t batches_flushed = 0;
    uint64_t total_append_ns = 0;
};

class JTextSplitEventLog {
public:
    JTextSplitEventLog([[maybe_unused]] std::string_view base_name,
                       size_t int_count,
                       size_t dbl_count,
                       PersistMode mode = PersistMode::All)
        : mode_(mode), int_count_(int_count), dbl_count_(dbl_count)
    {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
        std::string base(base_name);

        main_path_ = base + ".jtext";
        ints_path_ = base + "_Ints.jtext";
        floats_path_ = base + "_Floats.jtext";

        main_.emplace(main_path_);
        ints_.emplace(ints_path_);
        floats_.emplace(floats_path_);

        // Enable high-throughput mode (10K) on all writers by default
        main_->enable_high_throughput_batching();
        ints_->enable_high_throughput_batching();
        floats_->enable_high_throughput_batching();

        write_headers_and_field_lists();
#endif
    }

    ~JTextSplitEventLog() { finalize(); }

    void append_event([[maybe_unused]] size_t event_id,
                      size_t /*thread_id*/,
                      size_t /*per_thread_event_id*/,
                      [[maybe_unused]] uint64_t flags,
                      std::string_view /*category*/,
                      std::string_view /*payload*/,
                      uint64_t /*ts*/,
                      [[maybe_unused]] const std::vector<int64_t>& ints,
                      [[maybe_unused]] const std::vector<double>& dbls)
    {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
        if (!main_) return;

        if (mode_ == PersistMode::KeeperOnly) {
            constexpr uint64_t KEEPER_BIT = 1ULL << 1;
            if ((flags & KEEPER_BIT) == 0) return;
        }

        auto t0 = std::chrono::steady_clock::now();

        // Main (minimal record)
        {
            JTextEntry e{event_id, '|', 0, {std::to_string(event_id)}, "", false, false};
            main_->append_entry(e);
            stats_.main_rows++;
        }

        // Ints
        if (int_count_ > 0) {
            std::vector<std::string> f;
            f.reserve(1 + int_count_);
            f.push_back(std::to_string(event_id));
            for (size_t i = 0; i < int_count_ && i < ints.size(); ++i) f.push_back(std::to_string(ints[i]));
            JTextEntry e{event_id, '|', 0, std::move(f), "", false, false};
            ints_->append_entry(e);
            stats_.ints_rows++;
        }

        // Floats
        if (dbl_count_ > 0) {
            std::vector<std::string> f;
            f.reserve(1 + dbl_count_);
            f.push_back(std::to_string(event_id));
            for (size_t i = 0; i < dbl_count_ && i < dbls.size(); ++i) f.push_back(std::format("{:.8g}", dbls[i]));
            JTextEntry e{event_id, '|', 0, std::move(f), "", false, false};
            floats_->append_entry(e);
            stats_.floats_rows++;
        }

        auto t1 = std::chrono::steady_clock::now();
        stats_.total_append_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
#endif
    }

    void flush() {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
        if (main_)  { main_->flush();  stats_.batches_flushed++; }
        if (ints_)  { ints_->flush();  stats_.batches_flushed++; }
        if (floats_){ floats_->flush(); stats_.batches_flushed++; }
#endif
    }

    void finalize() {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
        if (main_)  main_->finalize();
        if (ints_)  ints_->finalize();
        if (floats_) floats_->finalize();
#endif
    }

    [[nodiscard]] const JTextSplitEventLogStats& stats() const { return stats_; }

    [[nodiscard]] const std::string& main_file() const { return main_path_; }
    [[nodiscard]] const std::string& ints_file() const { return ints_path_; }
    [[nodiscard]] const std::string& floats_file() const { return floats_path_; }

private:
    void write_headers_and_field_lists() {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
        if (main_) {
            main_->set_purpose("ts_store main events");
            main_->write_header();
            main_->begin_section("Field List");
            main_->append_row(1, {"event_id"}, "Linking ID (use with _Ints and _Floats)");
            main_->begin_section("Data Section");
        }

        if (int_count_ > 0 && ints_) {
            ints_->set_purpose("ts_store integer metrics");
            ints_->write_header();
            ints_->begin_section("Field List");

            std::vector<std::string> fields;
            fields.push_back("event_id");
            for (size_t i = 0; i < int_count_; ++i) fields.push_back("int" + std::to_string(i));
            ints_->append_row(1, fields, "Linking ID + 9 integer metrics");

            ints_->begin_section("Data Section");
        }

        if (dbl_count_ > 0 && floats_) {
            floats_->set_purpose("ts_store double metrics");
            floats_->write_header();
            floats_->begin_section("Field List");

            std::vector<std::string> fields;
            fields.push_back("event_id");
            for (size_t i = 0; i < dbl_count_; ++i) fields.push_back("dbl" + std::to_string(i));
            floats_->append_row(1, fields, "Linking ID + 6 double metrics");

            floats_->begin_section("Data Section");
        }
#endif
    }

#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    std::optional<JTextWriter> main_;
    std::optional<JTextWriter> ints_;
    std::optional<JTextWriter> floats_;
#endif

    std::string main_path_, ints_path_, floats_path_;
    [[maybe_unused]] PersistMode mode_;
    [[maybe_unused]] size_t int_count_ = 0;
    [[maybe_unused]] size_t dbl_count_ = 0;

    JTextSplitEventLogStats stats_;
};

} // namespace jac::ts_store::inline_v001

