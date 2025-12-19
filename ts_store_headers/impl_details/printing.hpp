// ts_store/ts_store_headers/impl_details/printing.hpp
// v5 — Fully compatible with typed ValueT/TypeT/CategoryT

#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstring>

inline void print(std::ostream& os = std::cout,
                  [[maybe_unused]] int sort_mode = 0,
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
        os << "ts_store<...> is empty.\n\n";
        return;
    }

    constexpr int W_ID       = 12;
    constexpr int W_TIME     = 14;
    constexpr int W_THREAD   = 10;
    constexpr int PAD        = 2;

    // Dynamic widths based on actual storage
    constexpr int W_TYPE = std::max<int>(Config::type_size + 4, 20);
    constexpr int W_CAT  = std::max<int>(Config::category_size + 4, 20);
    constexpr int W_PAYLOAD = Config::buffer_size > 1024 ? 1024 : Config::buffer_size;

    const int total_width = W_ID + PAD + W_TIME + PAD + W_TYPE + PAD + W_CAT + PAD + W_THREAD + PAD + W_PAYLOAD + 10;

    os << "ts_store < \n"
       << "   Threads    = " << max_threads_ << "\n"
       << "   Events     = " << events_per_thread_ << "\n"
       << "   ValueT     = " << (TriviallyCopyableStringLike<typename Config::ValueT> ? "Trivially Copyable String" : "Character Buffer") << "\n"
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

    for (size_t i = 0; i < total && (max_rows == 0 || i < max_rows); ++i) {
        const auto id = ids[i];
        auto it = rows_.find(id);
        if (it == rows_.end()) continue;

        const auto& r = it->second;

        std::string ts_str = "-";                                     // default when no timestamp
        if constexpr (Config::use_timestamps) {                                // <-- compile-time check
            if (r.ts_us != 0) ts_str = std::to_string(r.ts_us);       // only if actually set
        }

        // Extract type/category as string_view
        std::string_view type_sv = [&]() -> std::string_view {
            if constexpr (std::is_same_v<typename Config::TypeT, std::string_view>) {
                return std::string_view(r.type_storage.data());
            } else {
                return std::string_view(r.type_storage);
            }
        }();

        std::string_view cat_sv = [&]() -> std::string_view {
            if constexpr (std::is_same_v<typename Config::CategoryT, std::string_view>) {
                return std::string_view(r.category_storage.data());
            } else {
                return std::string_view(r.category_storage);
            }
        }();

        std::string_view payload_sv = [&]() -> std::string_view {
            if constexpr (TriviallyCopyableStringLike<typename Config::ValueT>) {
                return std::string_view(r.value_storage);
            } else {
                return std::string_view(r.value_storage.data());
            }
        }();

        os << std::left
           << std::setw(W_ID)     << id
           << std::setw(W_TIME)   << ts_str
           << std::setw(W_TYPE)   << type_sv
           << std::setw(W_CAT)    << cat_sv
           << std::setw(W_THREAD) << r.thread_id
           << payload_sv.substr(0, W_PAYLOAD) << "\n";
    }

    os << std::string(total_width, '=') << "\n";
    os << "Total entries: " << total << " (expected: " << expected_size() << ")\n\n";
}