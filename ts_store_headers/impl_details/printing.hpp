// ts_store/ts_store_headers/impl_details/printing.hpp
// Final — beautiful, debug-friendly, zero-cost printing

#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstring>

inline void print(std::ostream& os = std::cout,
                  int sort_mode = 0,
                  size_t max_rows = 10'000) const
{
    std::shared_lock lock(data_mtx_);

    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& [id, _] : rows_)
        ids.push_back(id);

    std::sort(ids.begin(), ids.end());  // chronological

    const size_t total = ids.size();

    if (total == 0) {
        os << "ts_store<Buffer:" << BufferSize
           << ", Type:" << TypeSize
           << ", Cat:" << CategorySize
           << ", TS:" << (UseTimestamps ? "yes" : "no") << "> is empty.\n\n";
        return;
    }

    // Column widths
    constexpr int W_ID       = 12;
    constexpr int W_TIME     = 14;
    constexpr int W_TYPE     = TypeSize + 4;
    constexpr int W_CAT      = CategorySize + 4;
    constexpr int W_THREAD   = 10;
    constexpr int PAD        = 2;

    const int total_width = W_ID + PAD + W_TIME + PAD + W_TYPE + PAD + W_CAT + PAD + W_THREAD + PAD + BufferSize;

    // Header
    os << "ts_store<"
       << max_threads_ << " threads × " << events_per_thread_ << " events"
       << ", Buffer:" << BufferSize << "B"
       << ", Type:" << TypeSize << "B, Cat:" << CategorySize << "B"
       << ", TS:" << (UseTimestamps ? "on" : "off")
       << ", Expected:" << expected_size() << ">\n";
    os << std::string(total_width, '=') << "\n";

    os << std::left
       << std::setw(W_ID)     << "ID"
       << std::setw(W_TIME)   << "TIME (µs)"
       << std::setw(W_TYPE)   << "TYPE"
       << std::setw(W_CAT)    << "CATEGORY"
       << std::setw(W_THREAD)<< "THREAD"
       << "PAYLOAD\n";

    os << std::string(total_width, '-') << "\n";

    // Print rows
    bool any_debug = false;
    for (size_t i = 0; i < total && (max_rows == 0 || i < max_rows); ++i) {
        const uint64_t id = ids[i];
        auto it = rows_.find(id);
        if (it == rows_.end()) continue;

        const auto& r = it->second;
        if (r.is_debug) any_debug = true;

        std::string ts_str = UseTimestamps && r.ts_us
            ? std::to_string(r.ts_us)
            : std::string("-");

        os << std::left
           << std::setw(W_ID)     << id
           << std::setw(W_TIME)   << ts_str
           << std::setw(W_TYPE)    << r.type
           << std::setw(W_CAT)    << r.category
           << std::setw(W_THREAD) << r.thread_id
           << r.value << "\n";

        // Insert breathing room only in debug mode and large dumps
        if (any_debug && total > 2000 && (i + 1) % 1000 == 0) {
            os << "\n";
        }
    }

    os << std::string(total_width, '=') << "\n";
    os << "Total entries: " << total
       << "  (expected: " << expected_size() << ")\n\n";
}