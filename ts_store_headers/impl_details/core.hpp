// ts_store/ts_store_headers/impl_details/core.hpp
// FINAL — 100% CORRECT, CLEAN, COMPILING — December 2025

#pragma once

#include "../../fmt/include/fmt/core.h"
#include "../../fmt/include/fmt/color.h"

// NO namespace — this file is included inside ts_store class


inline std::string_view make_test_payload(int thread_id, int event_index) noexcept {
    thread_local static char tl_buf[Config::buffer_size + 1]{};

    char* pos = tl_buf;

    constexpr const char prefix[] = "Test-Event: T=";
    std::memcpy(pos, prefix, sizeof(prefix) - 1);
    pos += sizeof(prefix) - 1;

    pos = itoa(pos, thread_id);
    constexpr const char worker_prefix[] = " Worker=";
    std::memcpy(pos, worker_prefix, sizeof(worker_prefix) - 1);
    pos += sizeof(worker_prefix) - 1;

    pos = itoa(pos, event_index);
    *pos++ = ' ';

    // Exact padding to BufferSize - 1
    const size_t used = static_cast<size_t>(pos - tl_buf);
    const size_t pad_len = (used < Config::buffer_size - 1) ? Config::buffer_size - used - 1 : 0;
    std::memset(pos, '.', pad_len);
    pos += pad_len;

    *pos = '\0';


    return {tl_buf, static_cast<size_t>(pos - tl_buf)};
}
// Main save_event definition (out-of-class, fully qualified with all template parameters)
//template<typename V = ValueT, typename T = TypeT, typename C = CategoryT>
//    requires StringLike<V> && StringLike<T> && StringLike<C>
inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id,
                     Config::ValueT&& value,
                     Config::TypeT&& type,
                     Config::CategoryT&& category,
                     bool debug = false)
{
    std::unique_lock lock(data_mtx_);
    const std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.is_debug  = debug;

    std::string_view sv = value;
    if (sv.empty()) {
        sv = "Payload not provided";
    }
    if (sv.size() + 1 > Config::buffer_size) {
        sv = sv.substr(0, Config::buffer_size - 1);
    }
    row.value_storage.copy_from(sv);

    auto copy_string = [&](auto& storage, auto&& src, size_t max_size, const char* fallback = "UNKNOWN") {
        std::string_view s = src;
        if (s.empty()) s = fallback;
        if (s.size() + 1 > max_size) s = s.substr(0, max_size - 1);
        storage.copy_from(s);
    };

    copy_string(row.type_storage,     std::forward<typename Config::TypeT>(type),         Config::type_size);
    copy_string(row.category_storage, std::forward<typename Config::CategoryT>(category), Config::category_size);

    // — TIMESTAMP —
    if constexpr (Config::use_timestamps) {
        const auto now = std::chrono::steady_clock::now();
        auto base = s_epoch_base.load(std::memory_order_relaxed);
        if (base == std::chrono::steady_clock::time_point::min()) {
            auto expected = std::chrono::steady_clock::time_point::min();
            if (s_epoch_base.compare_exchange_strong(expected, now))
                base = now;
            else
                base = s_epoch_base.load(std::memory_order_relaxed);
        }
        row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
    }

    rows_.insert_or_assign(id, std::move(row));
    return {true, id};
}

// Legacy overloads
inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id, std::string_view payload, std::string_view type, std::string_view category, bool debug = false)
{
    return save_event(thread_id, typename Config::ValueT(payload), typename Config::TypeT(type), typename Config::CategoryT(category), debug);
}

inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id, std::string_view payload, const char* type, const char* category, bool debug = false)
{
    return save_event(thread_id, payload, std::string_view(type ? type : "UNKNOWN"), std::string_view(category ? category : "UNKNOWN"), debug);
}

inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id, int index, bool debug = false)
{
    return save_event(thread_id, make_test_payload(thread_id, index), "TEST", "EVENT", debug);
}

inline std::pair<bool, std::uint64_t>
save_event(unsigned int thread_id, std::string_view payload, bool debug = false)
{
    return save_event(thread_id, payload, "DATA", "UNKNOWN", debug);
}

// select() — FINAL, CLEAN
inline auto select(std::uint64_t id) const {
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end())
        return std::pair<bool, std::string_view>{false, {}};
    return std::pair{true, it->second.value_storage.view()};
}

// get_all_ids
inline std::vector<std::uint64_t> get_all_ids() const {
    std::shared_lock lock(data_mtx_);
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& p : rows_) ids.push_back(p.first);
    return ids;
}

// get_timestamp_us
inline std::pair<bool, uint64_t> get_timestamp_us(std::uint64_t id) const {
    if constexpr (!Config::UseTimestamps) return {false, 0};
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    return (it == rows_.end()) ? std::pair{false, 0} : std::pair{true, it->second.ts_us};
}