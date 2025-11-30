// ts_store/ts_store_headers/impl_details/core.hpp
// Core claim/select with type & category — zero allocation, fixed-size strings

#pragma once
#include <string_view>
#include <chrono>
#include <shared_mutex>
#include <cstring>

inline size_t size() const noexcept {
    std::shared_lock lock(data_mtx_);
    return rows_.size();
}

inline std::vector<std::uint64_t> get_all_ids() const {
    std::shared_lock lock(data_mtx_);
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& [id, _] : rows_)
        ids.push_back(id);
    return ids;
}

// Main claim — accepts type and category as null-terminated strings
inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id,
      std::string_view payload,
      const char* type,
      const char* category,
      bool debug = false)
{
    if (payload.size() + 1 > BufferSize)
        return {false, 0};

    std::unique_lock lock(data_mtx_);

    std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    row_data row{};
    row.thread_id = thread_id;
    row.is_debug = debug;

    // Copy type
    std::strncpy(row.type, type, TypeSize - 1);
    row.type[TypeSize - 1] = '\0';

    // Copy category
    std::strncpy(row.category, category, CategorySize - 1);
    row.category[CategorySize - 1] = '\0';

    // Timestamp
    if constexpr (UseTimestamps || debug) {
        auto now = std::chrono::steady_clock::now();
        auto base = epoch_base.load(std::memory_order_relaxed);

        if (base == std::chrono::steady_clock::time_point::min()) {
            auto expected = std::chrono::steady_clock::time_point::min();
            if (epoch_base.compare_exchange_strong(expected, now))
                base = now;
            else
                base = epoch_base.load(std::memory_order_relaxed);
        }

        if constexpr (UseTimestamps) {
            row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
        }
    }

    // Copy payload
    std::memcpy(row.value, payload.data(), payload.size());
    row.value[payload.size()] = '\0';

    rows_.insert_or_assign(id, std::move(row));

    return {true, id};
}

// Legacy fast path — defaults to "DATA"/"TEST"
inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id, int index, bool debug = false)
{
    auto payload = FastPayload<BufferSize>::make(thread_id, index);
    return claim(thread_id, payload, "DATA", "TEST", debug);
}

// Legacy string-only path — kept for backward compatibility
inline std::pair<bool, std::uint64_t>
claim(unsigned int thread_id, std::string_view payload, bool debug = false)
{
    return claim(thread_id, payload, "DATA", "UNKNOWN", debug);
}

inline std::pair<bool, std::string_view> select(std::uint64_t id) const {
    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end())
        return {false, {}};
    return {true, std::string_view(it->second.value)};
}

inline std::pair<bool, uint64_t> get_timestamp_us(std::uint64_t id) const {
    if constexpr (!UseTimestamps)
        return {false, 0};

    std::shared_lock lock(data_mtx_);
    auto it = rows_.find(id);
    if (it == rows_.end() || it->second.ts_us == 0)
        return {false, 0};

    return {true, it->second.ts_us};
}