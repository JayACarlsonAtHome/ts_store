// ts_store/ts_store_headers/impl_details/printing.hpp
// Updated for std::string-based storage (dynamic allocation)
// Simplified column widths, direct std::string_view access

#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <limits>
#include <format>

inline void print(std::ostream& os = std::cout,
                  size_t max_rows = 10'000) const
{
    if (max_rows == 0) {
        max_rows = std::numeric_limits<size_t>::max();
    }

    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (uint64_t id = 0; id < rows_.size(); ++id) {
        ids.push_back(id);
    }

    std::sort(ids.begin(), ids.end());  // chronological order by ID

    const size_t total = ids.size();

    if (total == 0) {
        os << "ts_store is empty.\n\n";
        return;
    }

    constexpr int W_ID       = 12;
    constexpr int W_TIME     = 14;
    constexpr int W_THREAD   = 10;
    constexpr int PAD        = 2;

    // Fixed widths suitable for std::string (dynamic) usage
    constexpr int W_TYPE     = 10;
    constexpr int W_CAT      = 10;
    constexpr int W_PAYLOAD = 120;

    const int total_width = W_ID + PAD + W_TIME + PAD + W_TYPE + PAD + W_CAT + PAD + W_THREAD + PAD + W_PAYLOAD + 10;

    os << "ts_store <\n"
       << "   Threads    = " << max_threads_ << "\n"
       << "   Events     = " << events_per_thread_ << "\n"
       << "   ValueT     = std::string (dynamic allocation)\n"
       << "   Time Stamp = " << (Config::use_timestamps ? "On" : "Off") << ">\n";
    os << std::string(total_width, '=') << "\n";

    os << std::left
       << std::setw(W_ID)     << "ID"
       << std::setw(W_TIME)   << "TIME (µs)"
       << std::setw(W_TYPE)   << " TYPE"
       << std::setw(W_CAT)    << " CATEGORY"
       << std::setw(W_THREAD) << " THREAD"
       << " PAYLOAD\n";

    os << std::string(total_width, '-') << "\n";

    for (uint64_t id = 0; id < total && (max_rows == 0 || id < max_rows); ++id) {
        const auto& r = rows_[id];

        std::string ts_str = "-";                                     // default when no timestamp
        if constexpr (Config::use_timestamps) {
            if (r.ts_us != 0) ts_str = std::to_string(r.ts_us);
        }

        uint32_t t_padw = thread_id_width();
        uint32_t e_padw = events_id_width();
        std::string prefix = std::format("Test-Event: T={:>{}} W={:>{}} ",
                                         r.thread_id, t_padw, r.event_id, e_padw);

        std::string_view type_sv    = r.type_storage;
        std::string_view cat_sv     = r.category_storage;
        std::string_view payload_sv = r.value_storage;

        std::string full_line = prefix + std::string(payload_sv);
        os << std::left
           << std::setw(W_ID)     << id
           << std::setw(W_TIME)   << ts_str
           << std::setw(W_TYPE)   << type_sv
           << std::setw(W_CAT)    << cat_sv
           << "   "
           << std::right
           << std::setw(W_THREAD) << r.thread_id
           << std::left
           << "   "
           << full_line.substr(0, W_PAYLOAD) << "\n";
    }

    os << std::string(total_width, '=') << "\n";
    if (max_rows == 0) {
        os << "Output Truncated to " << max_rows << ".\n";
    }
    os << "Note: Output is zero indexed, so it is expected that last entry is " << expected_size()-1 << ".\n";
    os << "Total entries: " << total << " (expected: " << expected_size() << ")\n";
    os << std::endl;
}