// ts_store.hpp
// FINAL — CORRECT — ZERO visibility races + compiles cleanly

#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include "../GTL/include/gtl/phmap.hpp"

template <
    size_t Threads,
    size_t WorkersPerThread,
    size_t BufferSize = 80,
    bool UseTimestamps = true
>
class ts_store {
public:
    static constexpr size_t ExpectedSize = Threads * WorkersPerThread;

private:
    struct row_data {
        unsigned int thread_id{0};
        bool is_debug{false};
        char value[BufferSize];
        uint64_t ts_us{0};
    };

    static inline std::atomic<std::chrono::steady_clock::time_point> epoch_base{
        std::chrono::steady_clock::time_point::min()
    };

    std::atomic<std::uint64_t> next_id_{0};
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;
    std::vector<std::uint64_t> claimed_ids_;
    bool useTS_{UseTimestamps};

public:
    /*
    explicit ts_store() = default;

    size_t size() const {
        std::shared_lock lock(data_mtx_);
        return rows_.size();
    }

    void clear_claimed_ids() {
        std::unique_lock lock(data_mtx_);
        claimed_ids_.clear();
    }

    std::pair<bool, std::uint64_t> claim(unsigned int thread_id, std::string_view payload, bool debug = false) {
        if (payload.size() + 1 > BufferSize) return {false, 0};

        std::unique_lock lock(data_mtx_);
        std::uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

        row_data row{};
        row.thread_id = thread_id;
        row.is_debug = debug;

        if (useTS_ || debug) {
            auto now = std::chrono::steady_clock::now();
            auto base = epoch_base.load(std::memory_order_relaxed);
            if (base == std::chrono::steady_clock::time_point::min()) {
                auto expected = std::chrono::steady_clock::time_point::min();
                epoch_base.compare_exchange_strong(expected, now);
                base = epoch_base.load(std::memory_order_relaxed);
            }
            row.ts_us = std::chrono::duration_cast<std::chrono::microseconds>(now - base).count();
        }

        std::memcpy(row.value, payload.data(), payload.size());
        row.value[payload.size()] = '\0';

        rows_.insert_or_assign(id, std::move(row));

        // FIXED: publish claimed ID while still holding the exclusive lock
        claimed_ids_.push_back(id);

        return {true, id};
    }

    std::pair<bool, std::string_view> select(std::uint64_t id) const {
        std::shared_lock lock(data_mtx_);
        auto it = rows_.find(id);
        if (it == rows_.end()) return {false, {}};
        return {true, std::string_view(it->second.value)};
    }

    std::pair<bool, uint64_t> get_timestamp_us(uint64_t id) const {
        std::shared_lock lock(data_mtx_);
        auto it = rows_.find(id);
        if (it == rows_.end() || it->second.ts_us == 0) {
            return {false, 0};
        }
        return {true, it->second.ts_us};
    }
*/
    #include "impl_details/core.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"

};