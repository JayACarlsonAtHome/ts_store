// ts_store/ts_store_headers/ts_store.hpp
// v4 — 1.8M+ ops/sec — unbreakable — final form

#pragma once

#include <variant>
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
#include <sstream>
#include <sys/sysinfo.h>

#include "../GTL/include/gtl/phmap.hpp"
#include "impl_details/fast_payload.hpp"
#include "impl_details/memory_guard.hpp"

namespace jac::ts_store::inline_v001 {

template <
    size_t BufferSize    = 100,
    size_t TypeSize      = 16,
    size_t CategorySize  = 32,
    bool   UseTimestamps = true
>
class ts_store {
private:
    struct row_data {
        unsigned int thread_id{0};
        bool         is_debug{false};
        char         type[TypeSize]{};
        char         category[CategorySize]{};
        char         value[BufferSize]{};
        std::conditional_t<UseTimestamps, uint64_t, std::monostate> ts_us{};
    };

    const uint32_t max_threads_;
    const uint32_t events_per_thread_;

public:
    [[nodiscard]] uint64_t expected_size() const noexcept {
        return uint64_t(max_threads_) * events_per_thread_;
    }

    void clear() {
        std::unique_lock lock(data_mtx_);
        rows_.clear();
        next_id_.store(0, std::memory_order_relaxed);
        // timestamp epoch stays — it's global and correct
    }

    explicit ts_store(uint32_t max_threads, uint32_t events_per_thread)
        : max_threads_(max_threads)
        , events_per_thread_(events_per_thread)
    {
        if (max_threads == 0 || events_per_thread == 0)
            throw std::invalid_argument("ts_store: thread/event count must be > 0");

        rows_.reserve(expected_size() * 2);

        if constexpr (UseTimestamps) {
            const auto min_time = std::chrono::steady_clock::time_point::min();
            auto current = epoch_base.load(std::memory_order_relaxed);
            if (current == min_time) {
                epoch_base.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
            }
        }

        static const memory_guard guard(max_threads_, events_per_thread_, BufferSize, TypeSize, CategorySize, UseTimestamps);
        (void)guard;
    }

private:
    static inline std::atomic<std::chrono::steady_clock::time_point> epoch_base{
        std::chrono::steady_clock::time_point::min()
    };

    std::atomic<std::uint64_t> next_id_{0};
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;
    const bool useTS_{UseTimestamps};

public:
    // Core API — all claims here
    #include "impl_details/core.hpp"

    // Testing & diagnostics
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/press_to_cont.hpp"

    // REMOVED: duplicate claim() and size() — they are in core.hpp
    // REMOVED: duplicate fast path — now in core.hpp only
};

} // end of namespace jac::ts_store::inline_v001