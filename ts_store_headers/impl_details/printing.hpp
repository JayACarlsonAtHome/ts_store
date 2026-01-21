// ts_store/ts_store_headers/impl_details/printing.hpp
// Updated for std::string-based storage (dynamic allocation)
// Simplified column widths, direct std::string_view access

#pragma once

//#include <iostream>
//#include <iomanip>
//#include <string>
//#include <string_view>
//#include <algorithm>
//#include <vector>
//#include <cstdint>
//#include <limits>
//#include <format>
//#include "ansi_colors.hpp"

inline void print(std::ostream& os = std::cout,
                  size_t max_rows = 10'000) const
{
    const uint32_t w_id     = id_width();
    const uint32_t w_time   = Config::use_timestamps ? 18 : 10;
    const uint32_t w_type   = 5;
    const uint32_t w_cat    = 5;
    const uint32_t w_thread = thread_id_width();
    const uint32_t w_event  = events_id_width();


    constexpr size_t kBufferTweek = 31;  // "Test-Event: T=" (11) + " W=" (4) + " " (1) + safety
    const size_t total_width =
        static_cast<size_t>(w_id)     +
        static_cast<size_t>(w_time)   +
        static_cast<size_t>(w_type)   +
        static_cast<size_t>(w_cat)    +
        static_cast<size_t>(w_thread) +
        static_cast<size_t>(w_thread) +   // Yes, we really need this twice...
        static_cast<size_t>(w_event)  +

kMaxStoredPayloadLength +
        kBufferTweek;


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

    os << ansi::bold_white << "ts_store <" << ansi::reset << '\n'
       << "   Threads    = " << get_max_threads() << '\n'
       << "   Events     = " << get_max_events() << '\n'
       << "   ValueT     = std::string (dynamic allocation)\n"
       << "   Time Stamp = " << (Config::use_timestamps ? "On" : "Off") << ">\n";

    os << ansi::dim << std::string(total_width, '=') << ansi::reset << '\n';

    os << ansi::bold << std::right << std::setw(w_id)   << "ID"
       << "  "  << std::setw(static_cast<int>(w_time))   << "TIME (µs)"
       << "  "  << std::setw(static_cast<int>(w_type))   << "TYPE"
       << "   " << std::setw(static_cast<int>(w_cat))    << "CATEGORY"
       << "   "  << std::setw(static_cast<int>(w_thread)) << "THREAD"
       << "   "  << std::setw(static_cast<int>(w_event))  << "EVENT"
       << "     PAYLOAD"
       << ansi::reset << '\n';

    os << ansi::dim << std::string(total_width, '-') << ansi::reset << '\n';
    for (uint64_t id = 0; id < total && (max_rows == 0 || id < max_rows); ++id) {
        const auto& r = rows_[id];

        std::string ts_str = "-";
        if constexpr (Config::use_timestamps) {
            if (r.ts_us != 0) ts_str = std::to_string(r.ts_us);
        }

        std::string_view type_sv    = r.type_storage;
        std::string_view cat_sv     = r.category_storage;
        std::string_view payload_sv = r.value_storage;

        os << ansi::bold_white << std::right << std::setw(static_cast<int>(w_id))     << id          << ansi::reset
           << ansi::gray       << " "                                                                << ansi::reset
           << ansi::cyan       << std::right << std::setw(static_cast<int>(w_time))   << ts_str      << ansi::reset
           << ansi::gray       << "  "                                                                << ansi::reset
           << ansi::green      << std::left  << std::setw(static_cast<int>(w_type))   << type_sv     << ansi::reset
           << ansi::gray       << "  "                                                               << ansi::reset
           << ansi::magenta    << std::left  << std::setw(static_cast<int>(w_cat))    << cat_sv      << ansi::reset
           << ansi::gray       << "     "                                                            << ansi::reset
           << ansi::magenta    << std::right << std::setw(static_cast<int>(w_thread)) << r.thread_id << ansi::reset
           << ansi::gray       << "     "                                                            << ansi::reset
           << ansi::magenta    << std::right << std::setw(static_cast<int>(w_event))  << r.event_id  << ansi::reset
           << ansi::gray       << "     "                                                            << ansi::reset
           << ansi::yellow     << payload_sv                                                         << ansi::reset
           << '\n';
    }
    os << ansi::dim << std::string(total_width, '=') << ansi::reset << '\n';


    const size_t display_limit = max_rows;
    if (max_rows == 0) {
        max_rows = std::numeric_limits<size_t>::max();
    }
    if (display_limit != 0 && display_limit < total) {
        os << ansi::dim << "Output Truncated to " << display_limit << " rows (of " << total << ")." << ansi::reset << '\n';
    }

    os << ansi::dim << "Note: Output is zero indexed, so it is expected that last entry is " << (expected_size() - 1) << "." << ansi::reset << '\n';
    os << ansi::dim << "Total entries: " << total << " (expected: " << expected_size() << ")" << ansi::reset << '\n';
    os << std::endl;
}
