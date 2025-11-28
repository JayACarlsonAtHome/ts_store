// ts_store_concurrent.cpp

#include "../ts_store_headers/ts_store.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <array>

using namespace std::chrono;
using Clock = steady_clock;

// ──────────────────────────────────────────────────────────────
// Global, inline variables (C++17) – no reallocation, no crash
// ──────────────────────────────────────────────────────────────
alignas(64) inline std::atomic<size_t> log_stream_write_pos{0};
constexpr int    WRITER_THREADS = 500;
constexpr int    OPS_PER_THREAD = 100;
constexpr size_t MAX_ENTRIES = WRITER_THREADS * OPS_PER_THREAD + 1000;
constexpr size_t BUFFER_SIZE        = 100;
constexpr bool   USE_TIMESTAMPS     = true;
inline std::array<uint64_t, MAX_ENTRIES> log_stream_array{};

inline std::vector<std::thread> writers;
inline std::atomic<size_t> total_written{0};
// ──────────────────────────────────────────────────────────────

int main() {
    ts_store<WRITER_THREADS, OPS_PER_THREAD, BUFFER_SIZE, USE_TIMESTAMPS> store;

    auto writer_start = Clock::now();
    auto start_us = duration_cast<microseconds>(writer_start.time_since_epoch()).count();
    std::cout << "Writer start time : " << std::right << std::setw(14) << start_us << " µs\n";

    for (int t = 0; t < WRITER_THREADS; ++t) {
        writers.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                auto payload = "t" + std::to_string(t) + "-op" + std::to_string(i);
                auto [ok, id] = store.claim(t, payload, true);
                if (ok) {
                    size_t pos = log_stream_write_pos.fetch_add(1, std::memory_order_relaxed);
                    log_stream_array[pos] = id;
                    total_written.fetch_add(1, std::memory_order_release);
                }
            }
        });
    }

    while (total_written.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();

    auto reader_start = Clock::now();
    auto reader_start_us = duration_cast<microseconds>(reader_start.time_since_epoch()).count();
    std::cout << "Reader start time : " << std::right << std::setw(14) << reader_start_us << " µs\n";
    std::cout << "Reader start lag  : " << std::right << std::setw(14) << (reader_start_us - start_us) << " µs\n";

    std::atomic<long long> hits{0};
    std::atomic<long long> misses{0};

    std::thread tail_reader([&]() {
        size_t last_read = 0;

        while (total_written.load(std::memory_order_acquire) < WRITER_THREADS * OPS_PER_THREAD ||
               last_read < log_stream_write_pos.load(std::memory_order_acquire)) {

            size_t current_end = log_stream_write_pos.load(std::memory_order_acquire);

            while (last_read < current_end) {
                uint64_t id = log_stream_array[last_read++];
                auto [ok, _] = store.select(id);
                (ok ? hits : misses).fetch_add(1, std::memory_order_relaxed);
            }

            if (last_read >= current_end)
                std::this_thread::sleep_for(microseconds(50));
        }
    });

    for (auto& w : writers) w.join();

    auto writer_stop = Clock::now();
    auto writer_stop_us = duration_cast<microseconds>(writer_stop.time_since_epoch()).count();
    std::cout << "Writer stop time  : " << std::right << std::setw(14) << writer_stop_us << " µs\n";

    tail_reader.join();

    auto reader_stop = Clock::now();
    auto reader_stop_us = duration_cast<microseconds>(reader_stop.time_since_epoch()).count();
    auto finish_lag = reader_stop_us - writer_stop_us;

    std::cout << "Reader stop time  : " << std::right << std::setw(14) << reader_stop_us << " µs\n";
    std::cout << "Reader finish lag : " << std::right << std::setw(14) << finish_lag << " µs\n\n";

    std::cout << "Aggressive tail-reader: " << hits << " hits, " << misses << " misses (should be 0)\n";

    auto all_ids = store.get_claimed_ids_sorted(0);
    int final_ok = 0;
    for (auto id : all_ids)
        if (store.select(id).first) ++final_ok;

    std::cout << "Final post-write check: " << final_ok << "/" << all_ids.size() << "\n";
    store.show_duration("Store");
    std::cout << "\nPress Enter to display the full sorted trace...\n";
    std::cin.get();

    store.print(std::cout, 0,MAX_ENTRIES);

    return 0;
}