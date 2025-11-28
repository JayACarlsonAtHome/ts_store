// Project: ts_store
// File Path: ts_store/ts_store_headers/ts_store.hpp
//
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
#include <sstream>
#include <sys/sysinfo.h>
#include "../GTL/include/gtl/phmap.hpp"

template <
    size_t Threads,
    size_t WorkersPerThread,
    size_t BufferSize = 80,
    bool UseTimestamps = true
>
class ts_store {
private:
    struct row_data {
        unsigned int thread_id{0};
        bool is_debug{false};
        char value[BufferSize];
        uint64_t ts_us{0};
    };

    #include "impl_details/memory_guard.h"


public:
    static constexpr size_t ExpectedSize = Threads * WorkersPerThread;

    ts_store() {
        (void)guard_instance;  // Ensures guard is constructed if not already
    }
    // Public helpers
    static constexpr size_t row_data_size() noexcept { return sizeof(row_data); }

private:
    static inline std::atomic<std::chrono::steady_clock::time_point> epoch_base{
        std::chrono::steady_clock::time_point::min()
    };

    std::atomic<std::uint64_t> next_id_{0};
    gtl::parallel_flat_hash_map<std::uint64_t, row_data> rows_;
    mutable std::shared_mutex data_mtx_;
    // claimed_ids_ removed – it was the global contention point
    // We now use rows_.size() and direct iteration – faster and correct
    bool useTS_{UseTimestamps};


public:
    #include "impl_details/abbr_guide.hpp"
    #include "impl_details/core.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
};