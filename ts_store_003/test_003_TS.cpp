// test_003.cpp — Aggressive tail-reader stress test (500 threads × 100 ops)

#include "../ts_store_headers/ts_store.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <array>
#include <format>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

// ———————————————————— Test configuration ————————————————————
constexpr int    WRITER_THREADS     = 500;
constexpr int    OPS_PER_THREAD     = 100;
constexpr size_t MAX_ENTRIES        = WRITER_THREADS * OPS_PER_THREAD;

alignas(64) inline std::atomic<size_t> log_stream_write_pos{0};
inline std::array<uint64_t, MAX_ENTRIES> log_stream_array{};
inline std::atomic<size_t> total_written{0};

using LogConfig = ts_store_config<true>;
using LogxStore = ts_store<LogConfig>;


int main() {
    LogxStore store(WRITER_THREADS, OPS_PER_THREAD);

    std::vector<std::thread> writers;
    writers.reserve(WRITER_THREADS);

    auto writer_start = steady_clock::now();
    std::cout << "Writer start time : "
              << duration_cast<microseconds>(writer_start.time_since_epoch()).count()
              << " µs\n";

    for (int t = 0; t < WRITER_THREADS; ++t) {
        writers.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                // Now matches verify_test_payloads() expectations

                std::string payload ( LogxStore::test_messages[i % LogxStore::test_messages.size()]);
                if (payload.size() < LogxStore::kMaxStoredPayloadLength) payload.append(LogxStore::kMaxStoredPayloadLength - payload.size(), '.');
                std::string type = std::string(LogxStore::types[i % LogxStore::types.size()]);
                std::string cat  = std::string( LogxStore::categories[t % LogxStore::categories.size()]);
                bool is_debug = true;
                auto [ok, id] = store.save_event(t, i, std::move(payload), std::move(type), std::move(cat), is_debug);
                if (ok) {
                    size_t pos = log_stream_write_pos.fetch_add(1, std::memory_order_relaxed);
                    if (pos < MAX_ENTRIES) {
                        log_stream_array[pos] = id;
                    }
                    total_written.fetch_add(1, std::memory_order_release);
                }
            }
        });
    }

    while (total_written.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();

    auto reader_start = steady_clock::now();
    auto lag_us = duration_cast<microseconds>(reader_start - writer_start).count();
    std::cout << "Reader start time : "
              << duration_cast<microseconds>(reader_start.time_since_epoch()).count()
              << " µs\n";
    std::cout << "Reader start lag  : " << lag_us << " µs\n";

    std::atomic<long long> hits{0};
    std::atomic<long long> misses{0};

    std::thread tail_reader([&]() {
        size_t last_read = 0;

        while (total_written.load(std::memory_order_acquire) < WRITER_THREADS * OPS_PER_THREAD ||
               last_read < log_stream_write_pos.load(std::memory_order_acquire)) {

            size_t current_end = log_stream_write_pos.load(std::memory_order_acquire);

            while (last_read < current_end && last_read < MAX_ENTRIES) {
                uint64_t id = log_stream_array[last_read++];
                auto [ok, _] = store.select(id);
                (ok ? hits : misses).fetch_add(1, std::memory_order_relaxed);
            }

            if (last_read >= current_end)
                std::this_thread::sleep_for(microseconds(10));
        }
    });

    for (auto& w : writers) w.join();

    auto writer_stop = steady_clock::now();
    std::cout << "Writer stop time  : "
              << duration_cast<microseconds>(writer_stop.time_since_epoch()).count()
              << " µs\n";

    tail_reader.join();

    auto reader_stop = steady_clock::now();
    auto finish_lag = duration_cast<microseconds>(reader_stop - writer_stop).count();

    std::cout << "Reader stop time  : "
              << duration_cast<microseconds>(reader_stop.time_since_epoch()).count()
              << " µs\n";
    std::cout << "Reader finish lag : " << finish_lag << " µs\n\n";

    std::cout << "Tail-reader result: " << hits << " hits, " << misses << " misses (should be 0)\n";

    // Final verification
    if (!store.verify_integrity()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        store.diagnose_failures();
        return 1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_test_payloads()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        store.diagnose_failures();
        return 1;
    }
#endif

    std::cout << "ALL 50,000 ENTRIES VERIFIED — ZERO CORRUPTION\n";
    store.show_duration("Store");
    std::cout << "\nPress Enter to display (truncated) trace logs...\n";
    std::cin.get();
    store.print(std::cout, 0);
    return 0;
}
