// ts_store_big_test.cpp
// Big stress test — 250 threads × 1000 events = 250,000 entries

#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <string>

int main()
{
    constexpr uint32_t num_threads       = 250;
    constexpr uint32_t events_per_thread   = 1000;

    // Buffer:1000B, Type:16B, Category:32B, timestamps on
    ts_store<1000, 16, 32, true> store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store]() {
            for (uint32_t i = 0; i < events_per_thread; ++i) {
                std::string payload = "t" + std::to_string(t) + "-" + std::to_string(i);

                // Using the new claim() with type/category
                auto [ok, id] = store.claim(t, payload, "STRESS", "BIGTEST");
                if (!ok) {
                    std::cerr << "CLAIM FAILED thread " << t << " event " << i << "\n";
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    const size_t expected = num_threads * events_per_thread;

    if (ids.size() != expected) {
        std::cout << "LOST ENTRIES: " << ids.size() << " vs expected " << expected << "\n";
        return 1;
    }

    // Optional: verify a few entries (full scan is slow but safe)
    for (size_t i = 0; i < std::min<size_t>(1000, ids.size()); ++i)
    {

        auto [ok, val] = store.select(ids[i]);
        if (!ok || val.empty()) {
            std::cout << "CORRUPTED OR MISSING ID: " << ids[i] << "\n";
            return 1;
        }
    }
    std::cout << "Big Test Passed: " << ids.size() << " entries perfect\n";
    return 0;
}
