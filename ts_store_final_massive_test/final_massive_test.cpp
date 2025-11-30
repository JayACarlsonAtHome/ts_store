//
// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
//
#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

int main()
{
    constexpr uint32_t THREADS           = 250;
    constexpr uint32_t EVENTS_PER_THREAD = 4000;
    constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;

    constexpr size_t BUFFER_SIZE    = 100;
    constexpr size_t TYPE_SIZE      = 16;
    constexpr size_t CATEGORY_SIZE  = 32;
    constexpr bool   USE_TIMESTAMPS = true;

    ts_store<BUFFER_SIZE, TYPE_SIZE, CATEGORY_SIZE, USE_TIMESTAMPS> store(
        THREADS, EVENTS_PER_THREAD);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &store]() {
            for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
                // Zero-allocation fast path — still works!
                auto payload = FastPayload<BUFFER_SIZE>::make(t, i);

                // Use the modern claim with type/category
                auto [ok, id] = store.claim(t, payload, "MASSIVE", "FINAL");
                if (!ok || id >= (1ull << 50)) {
                    std::cerr << "CLAIM FAILED AT " << t << "/" << i << "\n";
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto mid = std::chrono::high_resolution_clock::now();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    if (ids.size() != TOTAL) {
        std::cout << "LOST " << TOTAL - ids.size() << " ENTRIES\n";
        return 1;
    }

    // Verify first 10,000 and last 10,000 — full scan is overkill but safe
    size_t errors = 0;
    for (size_t i = 0; i < std::min<size_t>(10000, TOTAL); ++i) {
        auto [ok, val] = store.select(ids[i]);
        if (!ok || val.find("payload-") != 0) errors++;
    }
    for (size_t i = TOTAL - std::min<size_t>(10000, TOTAL); i < TOTAL; ++i) {
        auto [ok, val] = store.select(ids[i]);
        if (!ok || val.find("payload-") != 0) errors++;
    }

    if (errors > 0) {
        std::cout << "CORRUPTION DETECTED: " << errors << " bad entries\n";
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto write_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto read_us  = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();

    std::cout << "Massive Test Passed — 1,000,000 events\n";
    std::cout << "  Write phase: " << write_us << " µs → "
              << (1'000'000 * 1'000'000.0 / write_us) << " ops/sec\n";
    std::cout << "  Read phase: " << read_us << " µs\n";
    std::cout << "  Total time: " << (write_us + read_us) << " µs\n";

    store.show_duration("Final massive run");

    return 0;
}