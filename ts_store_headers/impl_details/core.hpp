// ts_store/ts_store_headers/impl_details/core.hpp
// Updated for dynamic std::string storage (no fixed buffers, no copy_from, no size limits)
// NO namespace — this file is included inside ts_store class

inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id,
           unsigned int event_id,                  // ← new
           Config::ValueT&& value,
           Config::TypeT&& type,
           Config::CategoryT&& category,
           bool debug = false)
{
    const std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.event_id  = event_id;                      // ← store it
    row.is_debug  = debug;

    row.value_storage     = std::forward<typename Config::ValueT>(value);
    row.type_storage      = std::forward<typename Config::TypeT>(type);
    row.category_storage  = std::forward<typename Config::CategoryT>(category);

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
        row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
    }
    rows_[id] = std::move(row);
    return {true, id};
}

inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id,
           Config::ValueT&& value,
           Config::TypeT&& type,
           Config::CategoryT&& category,
           bool debug = false)
{
    const std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.is_debug  = debug;

    // Direct move/assignment — std::string handles everything dynamically
    row.value_storage = std::forward<typename Config::ValueT>(value);
    if (row.value_storage.empty()) {
        row.value_storage = "Payload not provided";
    }

    row.type_storage = std::forward<typename Config::TypeT>(type);
    if (row.type_storage.empty()) {
        row.type_storage = "UNKNOWN";
    }

    row.category_storage = std::forward<typename Config::CategoryT>(category);
    if (row.category_storage.empty()) {
        row.category_storage = "UNKNOWN";
    }

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
        row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
    }

    rows_.insert_or_assign(id, std::move(row));
    return {true, id};
}


// select() — returns string_view into stored std::string
inline auto select(std::uint64_t id) const
{
    if (id >= rows_.size()) {
        return std::pair<bool, std::string_view>{false, {}};
    }
    return std::pair{true, std::string_view(rows_[id].value_storage)};
}

// get_all_ids
inline std::vector<std::uint64_t> get_all_ids() const
{
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& p : rows_) {
        ids.push_back(p.first);
    }
    return ids;
}

// get_timestamp_us
inline std::pair<bool, uint64_t> get_timestamp_us(std::uint64_t id) const
{
    if constexpr (!Config::use_timestamps) {
        return {false, 0};
    }
    if (id >= rows_.size()) {
        return {false, 0};
    }
    return {true, rows_[id].ts_us};
}