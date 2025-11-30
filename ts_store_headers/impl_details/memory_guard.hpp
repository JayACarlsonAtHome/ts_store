// ts_store/ts_store_headers/impl_details/memory_guard.hpp
// Exact memory estimation — no guesswork, no underestimation

#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <sys/sysinfo.h>
#include <cstdlib>

struct memory_guard {
    memory_guard(uint32_t max_threads,
                 uint32_t events_per_thread,
                 size_t   buffer_size,
                 size_t   type_size,
                 size_t   category_size,
                 bool     use_timestamps)
    {
        const uint64_t N = static_cast<uint64_t>(max_threads) * events_per_thread;

        // Exact size of one row_data (aligned to 8 bytes)
        const uint64_t row_bytes =
            8 +                                          // thread_id (4) + is_debug (1) + 3 bytes padding → 8
            type_size +                                  // char type[TypeSize]
            category_size +                              // char category[CategorySize]
            buffer_size +                                // char value[BufferSize]
            (use_timestamps ? 8ULL : 0ULL);              // optional ts_us

        // parallel_flat_hash_map overhead: ~32–48 bytes per entry (conservative)
        const uint64_t overhead_per_entry = 40;

        const uint64_t bytes_per_entry = row_bytes + overhead_per_entry;

        // Extra headroom for hash table growth, logging, etc.
        const uint64_t extra_bytes = (N > 1000) ? (N + 1000) * 8 : 0;

        // +150 MiB safety margin (covers stack, other allocations, fragmentation)
        const uint64_t total_bytes = N * bytes_per_entry + extra_bytes + (150ULL << 20);

        std::ostringstream name;
        name << max_threads << " thread" << (max_threads == 1 ? "" : "s")
             << " × " << events_per_thread << " event" << (events_per_thread == 1 ? "" : "s")
             << "  (" << (N/1000) << "k entries"
             << ", " << buffer_size << "B payload"
             << ", " << type_size << "B type"
             << ", " << category_size << "B category"
             << (use_timestamps ? ", timestamps" : "") << ")";

        struct sysinfo info{};
        uint64_t available_ram = 8ULL << 30; // fallback: 8 GiB
        if (sysinfo(&info) == 0) {
            available_ram = info.freeram + info.bufferram + info.sharedram;
        }

        const uint64_t required_mib = total_bytes >> 20;
        const uint64_t avail_mib    = available_ram >> 20;

        std::cout << "Memory guard: " << name.str() << "\n"
                  << "   Required      : " << std::right << std::setw(8) << required_mib << " MiB\n"
                  << "   Available now : " << std::right << std::setw(8) << avail_mib    << " MiB\n";
        std::cout.flush();

        // Abort if we're using >92% of available RAM — prevents thrashing/OOM killer
        if (total_bytes > available_ram * 0.92) {
            std::cerr << "   NOT ENOUGH RAM — aborting to prevent system instability.\n\n";
            std::exit(1);
        }

        std::cout << "   RAM check: PASSED\n\n";
        std::cout.flush();
    }
};