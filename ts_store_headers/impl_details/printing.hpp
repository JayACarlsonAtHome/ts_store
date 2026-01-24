// ts_store/ts_store_headers/impl_details/printing.hpp
// Updated for std::string-based storage (dynamic allocation)
// Simplified column widths, direct std::string_view access

#pragma once

inline void print(std::ostream& os = std::cout,
                  size_t max_rows = 10'000) const
{
    constexpr uint32_t num_columns = 7;
    constexpr uint32_t num_separators = num_columns - 1;  // 6

    const uint32_t w_id     = id_width();
    const uint32_t w_time   = Config::use_timestamps ? 18 : 10;
    const uint32_t w_type   = Config::max_type_length;
    const uint32_t w_xcat   = Config::max_category_length;
    const uint32_t w_thread = thread_id_width();
    const uint32_t w_event  = events_id_width();
    const uint32_t w_pad    = 3;

    const uint32_t w_id_eff     = std::max(w_id,     3u);   // "ID" is 2 chars
    const uint32_t w_time_eff   = std::max(w_time,   10u);  // "TIME (µs)" ~10 chars
    const uint32_t w_type_eff   = std::max(w_type,   4u);   // "TYPE" = 4
    const uint32_t w_xcat_eff   = std::max(w_xcat,   8u);   // "CATEGORY" = 8
    const uint32_t w_thread_eff = std::max(w_thread, 6u);   // "THREAD" = 6
    const uint32_t w_event_eff  = std::max(w_event,  5u);   // "EVENT" = 5
    const uint32_t w_payload_eff = Config::max_payload_length + 1;  // header "PAYLOAD" is shorter + \0

    const size_t total_width =
        static_cast<size_t>(w_id_eff) +
        static_cast<size_t>(w_time_eff) +
        static_cast<size_t>(w_type_eff) +
        static_cast<size_t>(w_xcat_eff) +
        static_cast<size_t>(w_thread_eff) +
        static_cast<size_t>(w_event_eff) +
        static_cast<size_t>(w_payload_eff) +
        num_separators * w_pad;

    if (max_rows == 0) {
        max_rows = std::numeric_limits<size_t>::max();
    }

    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (uint64_t id = 0; id < rows_.size(); ++id) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    const size_t total = ids.size();

    if (total == 0) {
        os << "ts_store is empty.\n\n";
        return;
    }

    os << ansi::bold_white << "ts_store <" << ansi::reset << '\n'
       << "   Threads    = " << get_max_threads() << '\n'
       << "   Events     = " << get_max_events() << '\n'
       << "   ValueT     = std::string(max len=" << Config::max_payload_length << ")\n"
       << "   Time Stamp = " << (Config::use_timestamps ? "On" : "Off") << ">\n";

    os << ansi::dim << std::string(total_width, '=') << ansi::reset << '\n';

    std::string space_pad = std::string(w_pad, ' ');

    std::string header_type = "TYPE";
    header_type.resize(w_type_eff, '.');
    std::string header_cat = " CATEGORY";
    header_cat.resize(w_xcat_eff +1 , '.');
    std::string header_payload = " PAYLOAD";
    header_payload.resize(w_payload_eff+1, '.');

    os << ansi::bold
       << std::right << std::setw(w_id_eff)     << "ID"        << space_pad
       << std::right << std::setw(w_time_eff)   << "TIME (µs)" << space_pad
       << std::left << std::setw(w_type_eff)    << header_type << "  "
       << std::left << std::setw(w_xcat_eff)    << header_cat  << space_pad
       << std::right << std::setw(w_thread_eff) << "THREAD"    << space_pad
       << std::right << std::setw(w_event_eff)  << "EVENT"     << space_pad
       << std::left << std::setw(w_payload_eff) << header_payload
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

        std::string typex_padded(type_sv);
        typex_padded.resize(w_type_eff, '.');
        std::string catxx_padded(cat_sv);
        catxx_padded.resize(w_xcat_eff, '.');
        std::string payld_padded(payload_sv);
        payld_padded.resize(w_payload_eff, '.');

        os << ansi::bold_white << std::right << std::setw(w_id)          << id           << "  " << ansi::reset
           << ansi::cyan       << std::right << std::setw(w_time)        << ts_str       << space_pad << ansi::reset
           << ansi::green      << std::left  << std::setw(w_type_eff)    << typex_padded << space_pad << ansi::reset
           << ansi::magenta    << std::left  << std::setw(w_xcat_eff)    << catxx_padded << space_pad << ansi::reset
           << ansi::magenta    << std::right << std::setw(w_thread_eff)  << r.thread_id  << space_pad << ansi::reset
           << ansi::magenta    << std::right << std::setw(w_event_eff)   << r.event_id   << space_pad << " " << ansi::reset
           << ansi::yellow     << std::left  << std::setw(w_payload_eff) << payld_padded << ansi::reset
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