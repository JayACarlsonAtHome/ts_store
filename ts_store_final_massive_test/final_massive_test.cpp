// ts_store_final_apocalypse.cpp
// If this passes, we are done for the rest of our lives.

#include "../ts_store.hpp" // Uses BufferSize=80 default, pair<bool,...> returns
#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>

int main() {
    constexpr int THREADS = 250;
    constexpr int OPS_PER_THREAD = 4000;        // 1,000,000 total events
    constexpr int TOTAL = THREADS * OPS_PER_THREAD;

    ts_store<80> store(true);
    store.reserve(TOTAL + 1000);                // plenty of headroom

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &store]() {
            std::mt19937 rng(t);
            std::string payload = "payload-" + std::to_string(t) + "-";

            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string s = payload + std::to_string(i);
                auto [ok, id] = store.claim(t, s);
                if (!ok || id >= 1ull << 50) {          // impossible unless broken
                    std::cerr << "CLAIM FAILED AT " << t << "/" << i << "\n";
                    std::abort();
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    auto mid = std::chrono::high_resolution_clock::now();

    // Every single ID must be present and correct
    auto ids = store.get_claimed_ids_sorted(0);
    if (ids.size() != TOTAL) {
        std::cout << "LOST " << TOTAL - ids.size() << " ENTRIES\n";
        return 1;
    }

    for (uint64_t i = 0; i < TOTAL; ++i) {
        auto [ok, val] = store.select(i);
        if (!ok || val.find("payload-") != 0) {
            std::cout << "CORRUPTION AT ID " << i << "\n";
            return 1;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto write_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto read_us  = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();

    std::cout << "Massive Test Passed\n";
    std::cout << "1,000,000 writes in " << write_us << " µs → "
              << (1'000'000 * 1'000'000.0 / write_us) << " ops/sec\n";
    std::cout << "1,000,000 sequential reads in " << read_us << " µs\n";

    return 0;
}