// ts_store/ts_store_headers/impl_details/core.hpp
// Updated for dynamic std::string storage (no fixed buffers, no copy_from, no size limits)
// NO namespace — this file is included inside ts_store class

inline std::pair<bool, size_t>
save_event(size_t thread_id,
           size_t event_id,
           Config::ValueT&& value,
           size_t event_flag_param = 0,
           Config::CategoryT&& category = "",
           bool debug = false
           //size_t user_flags = 0
)
{
    const size_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.event_id  = event_id;
    row.is_debug  = debug;

    row.value_storage = Config::utf8_truncate(value, Config::max_payload_length);
    row.category_storage = Config::utf8_truncate(category, Config::max_category_length);

    if (!row.value_storage.empty()) {
        flags_set_has_data(event_flag_param);
    } else {
        flags_clear_has_data(event_flag_param);
    }
    row.event_flags = event_flag_param;

    // — TIMESTAMP —
    if constexpr (Config::use_timestamps) {
        const auto now = std::chrono::steady_clock::now();
        auto base = s_epoch_base.load(std::memory_order_relaxed);
        if (base == std::chrono::steady_clock::time_point::min()) {
            auto expected = std::chrono::steady_clock::time_point::min();
            if (s_epoch_base.compare_exchange_strong(expected, now)) {
                base = now;
            } else {
                base = s_epoch_base.load(std::memory_order_relaxed);
            }
        }
        row.ts_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - base).count()
    );
    }

    rows_[id] = std::move(row);
    return {true, id};
}

// select() — returns string_view into stored std::string
inline auto select(size_t id) const
{
    if (id >= rows_.size()) {
        return std::pair<bool, std::string_view>{false, {}};
    }
    return std::pair{true, std::string_view(rows_[id].value_storage)};
}

// get_all_ids
inline std::vector<size_t> get_all_ids() const
{
    std::vector<size_t> ids;
    ids.reserve(rows_.size());
    for (const auto& p : rows_) {
        ids.push_back(p.first);
    }
    return ids;
}

// get_timestamp_us
inline std::pair<bool, size_t> get_timestamp_us(size_t id) const
{
    if constexpr (!Config::use_timestamps) {
        return {false, 0};
    }
    if (id >= rows_.size()) {
        return {false, 0};
    }
    return {true, rows_[id].ts_us};
}