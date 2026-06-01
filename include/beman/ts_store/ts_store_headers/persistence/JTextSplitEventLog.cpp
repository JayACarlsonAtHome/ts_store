#include "JTextSplitEventLog.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
#include "jText.h"
#endif

namespace jac::ts_store::inline_v001 {

namespace fs = std::filesystem;

JTextSplitEventLog::JTextSplitEventLog(
    std::string_view base_name,
    size_t int_count,
    size_t dbl_count,
    PersistMode mode
)
    : mode_(mode)
    , int_count_(int_count)
    , dbl_count_(dbl_count)
{
    if (base_name.empty()) {
        throw std::invalid_argument("JTextSplitEventLog: base_name cannot be empty");
    }

    main_path_ = std::format("{}.jtext", base_name);
    ints_path_ = std::format("{}_Ints.jtext", base_name);
    floats_path_ = std::format("{}_Floats.jtext", base_name);

    // Create the three writers (this opens the files)
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    main_writer_ = std::make_unique<jtext::JTextWriter>(main_path_);
    ints_writer_ = std::make_unique<jtext::JTextWriter>(ints_path_);
    floats_writer_ = std::make_unique<jtext::JTextWriter>(floats_path_);

    // Enable high-throughput auto-batching by default (10K as per project decision)
    main_writer_->enable_high_throughput_batching();
    ints_writer_->enable_high_throughput_batching();
    floats_writer_->enable_high_throughput_batching();
#else
    // Feature not enabled — writers stay null. All operations will be no-ops or throw.
#endif

    // Generate the Postgres SQL companion files immediately (early, before any events)
    write_sql_companions(base_name, int_count, dbl_count);

    // Write headers + Field Lists + Metadata sections right now
    write_all_headers(int_count, dbl_count);
}

JTextSplitEventLog::~JTextSplitEventLog() {
    if (!finalized_) {
        try {
            finalize();
        } catch (...) {
            // Best effort in destructor
        }
    }
}

JTextSplitEventLog::JTextSplitEventLog(JTextSplitEventLog&& other) noexcept = default;
JTextSplitEventLog& JTextSplitEventLog::operator=(JTextSplitEventLog&& other) noexcept = default;

void JTextSplitEventLog::append_event(
    size_t event_id,
    size_t thread_id,
    size_t per_thread_event_id,
    uint64_t raw_flags,
    std::string_view category,
    std::string_view payload,
    uint64_t timestamp_us,
    const std::vector<int64_t>& int_metrics,
    const std::vector<double>& dbl_metrics
) {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    if (!main_writer_) return;

    // Check KeeperRecord filtering
    if (mode_ == PersistMode::KeeperOnly) {
        // Bit 1 = KeeperRecord
        constexpr uint64_t KEEPER_MASK = 1ULL << 1;
        if ((raw_flags & KEEPER_MASK) == 0) {
            return; // skip this event
        }
    }

    // --- Main file ---
    jtext::JTextEntry main_entry;
    main_entry.number = event_id;
    main_entry.level_sep = '|';
    main_entry.fields = {
        std::to_string(thread_id),
        std::to_string(per_thread_event_id),
        std::format("0x{:016x}", raw_flags),
        category.empty() ? "" : std::string(category),
        payload.empty() ? "" : std::string(payload),
        std::to_string(timestamp_us)
    };
    main_writer_->append_entry(main_entry);

    // --- Ints file ---
    if (int_count_ > 0) {
        jtext::JTextEntry ints_entry;
        ints_entry.number = event_id;
        ints_entry.level_sep = '|';
        ints_entry.fields.reserve(1 + int_metrics.size());
        ints_entry.fields.push_back(std::to_string(event_id)); // linking ID first

        for (size_t i = 0; i < int_count_ && i < int_metrics.size(); ++i) {
            ints_entry.fields.push_back(std::to_string(int_metrics[i]));
        }
        ints_writer_->append_entry(ints_entry);
    }

    // --- Floats file ---
    if (dbl_count_ > 0) {
        jtext::JTextEntry floats_entry;
        floats_entry.number = event_id;
        floats_entry.level_sep = '|';
        floats_entry.fields.reserve(1 + dbl_metrics.size());
        floats_entry.fields.push_back(std::to_string(event_id));

        for (size_t i = 0; i < dbl_count_ && i < dbl_metrics.size(); ++i) {
            floats_entry.fields.push_back(std::format("{:.10g}", dbl_metrics[i]));
        }
        floats_writer_->append_entry(floats_entry);
    }
#endif
}

void JTextSplitEventLog::flush() {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    if (main_writer_) main_writer_->flush();
    if (ints_writer_) ints_writer_->flush();
    if (floats_writer_) floats_writer_->flush();
#endif
}

void JTextSplitEventLog::finalize() {
    if (finalized_) return;

#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    if (main_writer_) main_writer_->finalize();
    if (ints_writer_) ints_writer_->finalize();
    if (floats_writer_) floats_writer_->finalize();
#endif
    finalized_ = true;
}

void JTextSplitEventLog::write_sql_companions(
    std::string_view base_name,
    size_t int_count,
    size_t dbl_count
) {
    // Main SQL
    {
        std::string sql_path = std::format("{}.sql", base_name);
        std::ofstream sql(sql_path);
        sql << "-- Generated alongside " << base_name << ".jtext\n";
        sql << "-- \\i this file in psql or use as reference\n\n";
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << " (\n";
        sql << "    id BIGINT PRIMARY KEY,\n";
        sql << "    thread_id BIGINT,\n";
        sql << "    per_thread_event_id BIGINT,\n";
        sql << "    flags_raw BIGINT,\n";
        sql << "    category TEXT,\n";
        sql << "    payload TEXT,\n";
        sql << "    timestamp_us BIGINT\n";
        sql << ");\n\n";
        sql << "-- Source: " << base_name << ".jtext (jText format for headless inspection)\n";
    }

    // Ints SQL
    if (int_count > 0) {
        std::string sql_path = std::format("{}_Ints.sql", base_name);
        std::ofstream sql(sql_path);
        sql << "-- Generated alongside " << base_name << "_Ints.jtext\n\n";
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << "_ints (\n";
        sql << "    id BIGINT PRIMARY KEY,\n";
        for (size_t i = 0; i < int_count; ++i) {
            sql << "    int" << i << " BIGINT";
            if (i + 1 < int_count) sql << ",";
            sql << "\n";
        }
        sql << ");\n\n";
        sql << "-- Source: " << base_name << "_Ints.jtext\n";
    }

    // Floats SQL
    if (dbl_count > 0) {
        std::string sql_path = std::format("{}_Floats.sql", base_name);
        std::ofstream sql(sql_path);
        sql << "-- Generated alongside " << base_name << "_Floats.jtext\n\n";
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << "_floats (\n";
        sql << "    id BIGINT PRIMARY KEY,\n";
        for (size_t i = 0; i < dbl_count; ++i) {
            sql << "    dbl" << i << " DOUBLE PRECISION";
            if (i + 1 < dbl_count) sql << ",";
            sql << "\n";
        }
        sql << ");\n\n";
        sql << "-- Source: " << base_name << "_Floats.jtext\n";
    }
}

void JTextSplitEventLog::write_all_headers(size_t int_count, size_t dbl_count) {
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    if (!main_writer_) return;

    // Main header + Field List
    main_writer_->set_purpose("ts_store split event log (main)");
    main_writer_->write_header();

    main_writer_->begin_section("Field List");
    // Simple field list for main (no fancy alignment needed for high-volume logs)
    main_writer_->append_row(1, {"thread_id", "per_thread_event_id", "flags_raw", "category", "payload", "timestamp_us"}, "Core event columns");

    // Ints header + Field List
    if (int_count > 0 && ints_writer_) {
        ints_writer_->set_purpose("ts_store integer metrics");
        ints_writer_->write_header();
        ints_writer_->begin_section("Field List");

        std::vector<std::string> fields;
        fields.reserve(1 + int_count);
        fields.emplace_back("event_id");
        for (size_t i = 0; i < int_count; ++i) {
            fields.push_back("int" + std::to_string(i));
        }
        ints_writer_->append_row(1, fields, "Linking ID + integer metrics");
    }

    // Floats header + Field List
    if (dbl_count > 0 && floats_writer_) {
        floats_writer_->set_purpose("ts_store double metrics");
        floats_writer_->write_header();
        floats_writer_->begin_section("Field List");

        std::vector<std::string> fields;
        fields.reserve(1 + dbl_count);
        fields.emplace_back("event_id");
        for (size_t i = 0; i < dbl_count; ++i) {
            fields.push_back("dbl" + std::to_string(i));
        }
        floats_writer_->append_row(1, fields, "Linking ID + double metrics");
    }
#endif
}

} // namespace jac::ts_store::inline_v001
