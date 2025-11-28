
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/memory_guard.hpp
//


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
