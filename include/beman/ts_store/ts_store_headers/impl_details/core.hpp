// ts_store/ts_store_headers/impl_details/core.hpp
// Updated for dynamic std::string storage (no fixed buffers, no copy_from, no size limits)
// NO namespace — this file is included inside ts_store class

inline std::pair<bool, size_t>
save_event(size_t thread_id,
           size_t event_id,
           Config::ValueT&& value,
           size_t event_flag_param = 0,
           Config::CategoryT&& category = "",
           bool debug = false,
           std::array<int64_t, Config::the_IntMetrics> int_metrics = {},
           std::array<double,  Config::the_DblMetrics> dbl_metrics = {}
)
{
    const size_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.event_id  = event_id;
    row.is_debug  = debug;

    row.value_storage = Config::utf8_truncate(value, Config::max_payload_length);
    row.category_storage = Config::utf8_truncate(category, Config::max_category_length);

    row.int_metrics = std::move(int_metrics);
    row.dbl_metrics = std::move(dbl_metrics);

    if (!row.value_storage.empty()) {
        event_flag_param = flags_set_has_data(event_flag_param);
    } else {
        event_flag_param = flags_clear_has_data(event_flag_param);
    }

    // Set metric presence flags (simple non-zero heuristic; callers can also set the flags explicitly)
    if constexpr (Config::the_IntMetrics > 0) {
        bool has = false;
        for (auto v : row.int_metrics) { if (v != 0) { has = true; break; } }
        if (has) event_flag_param = set_metric_flag(event_flag_param, TsStoreFlags::MetricFlag::HasIntData);
    }
    if constexpr (Config::the_DblMetrics > 0) {
        bool has = false;
        for (auto v : row.dbl_metrics) { if (v != 0.0) { has = true; break; } }
        if (has) event_flag_param = set_metric_flag(event_flag_param, TsStoreFlags::MetricFlag::HasDblData);
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
        row.ts_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - base).count());
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