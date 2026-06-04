#include "JTextSplitEventLog.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <chrono>

#include "jText.h"

namespace jac::ts_store::inline_v001 {

namespace fs = std::filesystem;

struct JTextSplitEventLog::Impl {
    std::ofstream main_ofs;
    std::ofstream ints_ofs;
    std::ofstream floats_ofs;

    std::unique_ptr<JTextWriter> main_writer;
    std::unique_ptr<JTextWriter> ints_writer;
    std::unique_ptr<JTextWriter> floats_writer;

    std::string main_path;
    std::string ints_path;
    std::string floats_path;

    PersistMode mode;
    size_t int_count = 0;
    size_t dbl_count = 0;

    JTextSplitEventLogStats stats;
    bool finalized = false;
};

JTextSplitEventLog::JTextSplitEventLog(
    std::string_view base_name,
    size_t int_count,
    size_t dbl_count,
    PersistMode mode
)
    : impl_(std::make_unique<Impl>())
{
    if (base_name.empty()) {
        throw std::invalid_argument("JTextSplitEventLog: base_name cannot be empty");
    }

    auto& i = *impl_;
    i.mode = mode;
    i.int_count = int_count;
    i.dbl_count = dbl_count;

    i.main_path = std::format("{}.jtext", base_name);
    i.ints_path = std::format("{}_Ints.jtext", base_name);
    i.floats_path = std::format("{}_Floats.jtext", base_name);

    // Open files ourselves first so we can prepend the required standardized // header comments.
    // Then attach JTextWriter to the open stream (its # header comes after our // block).
    auto open_and_prep = [](std::ofstream& ofs, const std::string& path, std::string_view purpose) {
        ofs.open(path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            throw std::runtime_error("JTextSplitEventLog: failed to open " + path);
        }
        ::write_file_comment_header(ofs, path, purpose);
    };

    open_and_prep(i.main_ofs, i.main_path, "jText Data File");
    i.main_writer = std::make_unique<JTextWriter>(i.main_ofs);

    open_and_prep(i.ints_ofs, i.ints_path, "jText Field List File");
    i.ints_writer = std::make_unique<JTextWriter>(i.ints_ofs);

    open_and_prep(i.floats_ofs, i.floats_path, "jText Field List File");
    i.floats_writer = std::make_unique<JTextWriter>(i.floats_ofs);

    // High-throughput default (10k batching)
    i.main_writer->enable_high_throughput_batching();
    i.ints_writer->enable_high_throughput_batching();
    i.floats_writer->enable_high_throughput_batching();

    // Write SQL companions and headers immediately (early open requirement)
    write_sql_companions(base_name, int_count, dbl_count);
    write_all_headers(int_count, dbl_count);
}

JTextSplitEventLog::~JTextSplitEventLog() {
    if (impl_ && !impl_->finalized) {
        try { finalize(); } catch (...) {}
    }
}

JTextSplitEventLog::JTextSplitEventLog(JTextSplitEventLog&&) noexcept = default;
JTextSplitEventLog& JTextSplitEventLog::operator=(JTextSplitEventLog&&) noexcept = default;

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
    auto& i = *impl_;
    if (!i.main_writer) return;

    if (i.mode == PersistMode::KeeperOnly) {
        constexpr uint64_t KEEPER_MASK = 1ULL << 1;
        if ((raw_flags & KEEPER_MASK) == 0) return;
    }

    // Main record
    {
        JTextEntry e;
        e.number = event_id;
        e.level_sep = '|';
        e.fields = {
            std::to_string(thread_id),
            std::to_string(per_thread_event_id),
            std::format("0x{:016x}", raw_flags),
            std::string(category),
            std::string(payload),
            std::to_string(timestamp_us)
        };
        i.main_writer->append_entry(e);
        i.stats.main_rows++;
    }

    // Ints
    if (i.int_count > 0) {
        JTextEntry e;
        e.number = event_id;
        e.level_sep = '|';
        e.fields.reserve(1 + int_metrics.size());
        e.fields.push_back(std::to_string(event_id));
        for (auto v : int_metrics) e.fields.push_back(std::to_string(v));
        i.ints_writer->append_entry(e);
        i.stats.ints_rows++;
    }

    // Floats
    if (i.dbl_count > 0) {
        JTextEntry e;
        e.number = event_id;
        e.level_sep = '|';
        e.fields.reserve(1 + dbl_metrics.size());
        e.fields.push_back(std::to_string(event_id));
        for (auto v : dbl_metrics) e.fields.push_back(std::format("{:.10g}", v));
        i.floats_writer->append_entry(e);
        i.stats.floats_rows++;
    }
}

void JTextSplitEventLog::flush() {
    auto& i = *impl_;
    if (i.main_writer) i.main_writer->flush();
    if (i.ints_writer) i.ints_writer->flush();
    if (i.floats_writer) i.floats_writer->flush();
    i.stats.batches_flushed++;
}

void JTextSplitEventLog::finalize() {
    auto& i = *impl_;
    if (i.finalized) return;
    if (i.main_writer) i.main_writer->finalize();
    if (i.ints_writer) i.ints_writer->finalize();
    if (i.floats_writer) i.floats_writer->finalize();
    i.finalized = true;
}

const JTextSplitEventLogStats& JTextSplitEventLog::stats() const {
    return impl_->stats;
}

const std::string& JTextSplitEventLog::main_file() const { return impl_->main_path; }
const std::string& JTextSplitEventLog::ints_file() const { return impl_->ints_path; }
const std::string& JTextSplitEventLog::floats_file() const { return impl_->floats_path; }

void JTextSplitEventLog::write_sql_companions(
    std::string_view base_name,
    size_t int_count,
    size_t dbl_count
) {
    // (SQL generation logic - kept functional, no macros)
    {
        std::string sql_path = std::format("{}.sql", base_name);
        std::ofstream sql(sql_path);
        ::write_file_comment_header(sql, sql_path, "SQL Schema File");
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << " (\n"
            << "    id BIGINT PRIMARY KEY,\n"
            << "    thread_id BIGINT,\n"
            << "    per_thread_event_id BIGINT,\n"
            << "    flags_raw BIGINT,\n"
            << "    category TEXT,\n"
            << "    payload TEXT,\n"
            << "    timestamp_us BIGINT\n"
            << ");\n\n";
    }

    if (int_count > 0) {
        std::string sql_path = std::format("{}_Ints.sql", base_name);
        std::ofstream sql(sql_path);
        ::write_file_comment_header(sql, sql_path, "SQL Schema File");
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << "_ints (\n"
            << "    id BIGINT PRIMARY KEY,\n";
        for (size_t i = 0; i < int_count; ++i) {
            sql << "    int" << i << " BIGINT";
            if (i + 1 < int_count) sql << ",";
            sql << "\n";
        }
        sql << ");\n\n";
    }

    if (dbl_count > 0) {
        std::string sql_path = std::format("{}_Floats.sql", base_name);
        std::ofstream sql(sql_path);
        ::write_file_comment_header(sql, sql_path, "SQL Schema File");
        sql << "CREATE TABLE IF NOT EXISTS " << base_name << "_floats (\n"
            << "    id BIGINT PRIMARY KEY,\n";
        for (size_t i = 0; i < dbl_count; ++i) {
            sql << "    dbl" << i << " DOUBLE PRECISION";
            if (i + 1 < dbl_count) sql << ",";
            sql << "\n";
        }
        sql << ");\n\n";
    }
}

void JTextSplitEventLog::write_all_headers(size_t int_count, size_t dbl_count) {
    auto& i = *impl_;
    if (!i.main_writer) return;

    i.main_writer->set_purpose("ts_store split event log (main)");
    i.main_writer->write_header();
    i.main_writer->begin_section("Field List");
    i.main_writer->append_row(1, {"thread_id", "per_thread_event_id", "flags_raw", "category", "payload", "timestamp_us"}, "Core event columns");

    if (int_count > 0 && i.ints_writer) {
        i.ints_writer->set_purpose("ts_store integer metrics");
        i.ints_writer->write_header();
        i.ints_writer->begin_section("Field List");

        std::vector<std::string> fields = {"event_id"};
        for (size_t k = 0; k < int_count; ++k) fields.push_back("int" + std::to_string(k));
        i.ints_writer->append_row(1, fields, "Linking ID + integer metrics");
    }

    if (dbl_count > 0 && i.floats_writer) {
        i.floats_writer->set_purpose("ts_store double metrics");
        i.floats_writer->write_header();
        i.floats_writer->begin_section("Field List");

        std::vector<std::string> fields = {"event_id"};
        for (size_t k = 0; k < dbl_count; ++k) fields.push_back("dbl" + std::to_string(k));
        i.floats_writer->append_row(1, fields, "Linking ID + double metrics");
    }
}

} // namespace jac::ts_store::inline_v001
