// ts_store.hpp
// FINAL — ZERO races, self-protecting, perfect diagnostics, compiles forever

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

    // ————————————————————————————————————————
    // MEMORY GUARD — runs automatically on construction
    // ————————————————————————————————————————
    struct memory_guard {
        memory_guard() {
            constexpr uint64_t N = Threads * WorkersPerThread;

            constexpr uint64_t row_bytes = sizeof(row_data);
            constexpr uint64_t bytes_per_entry = row_bytes + 40;
            constexpr uint64_t log_bytes = (N > 1000) ? (N + 1000) * 8 : 0;
            constexpr uint64_t total_bytes = N * bytes_per_entry + log_bytes + (150ULL << 20);

            std::ostringstream name;
            name << Threads << "-thread" << (Threads == 1 ? " " : "s ")
                 << WorkersPerThread << "-op" << (WorkersPerThread == 1 ? "" : "s")
                 << " test (" << (N/1000) << "k entries)" << (UseTimestamps ? " [timestamps]" : "");

            struct sysinfo info{};
            uint64_t avail = (sysinfo(&info) == 0)
                ? info.freeram + info.bufferram + info.sharedram
                : 8ULL << 30;

            std::cout << "Memory guard: " << name.str() << "\n"
                      << "   Required      : " << std::right << std::setw(8) << (total_bytes >> 20) << " MiB\n"
                      << "   Available now : " << std::right << std::setw(8) << (avail >> 20)       << " MiB\n";
            std::cout.flush();  // <-- FORCES OUTPUT IMMEDIATELY

            if (total_bytes > avail * 0.92) {
                std::cerr << "   NOT ENOUGH RAM — aborting safely.\n\n";
                std::exit(1);
            }
            std::cout << "   RAM check: PASSED\n\n";
            std::cout.flush();
        }
    };

    // Run guard exactly once per type
    static inline const memory_guard guard_instance{};

public:
    static constexpr size_t ExpectedSize = Threads * WorkersPerThread;

    // Default constructor — guard already ran
    // Now runs guard on first construction (visible!)
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
    std::vector<std::uint64_t> claimed_ids_;
    bool useTS_{UseTimestamps};

public:
    #include "impl_details/core.hpp"
    #include "impl_details/sorting.hpp"
    #include "impl_details/testing.hpp"
    #include "impl_details/printing.hpp"
    #include "impl_details/duration.hpp"
};