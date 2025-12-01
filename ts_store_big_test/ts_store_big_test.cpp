// ts_store_big_test.cpp
#include "../ts_store_headers/ts_store.hpp"

#include <thread>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdio>

using namespace jac::ts_store::inline_v001;

int main()
{

    constexpr uint32_t num_threads       = 250;
    constexpr uint32_t events_per_thread = 1000;

    ts_store<1000, 16, 32, true> store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store]() {
            for (uint32_t i = 0; i < events_per_thread; ++i) {
                char payload_buf[64];
                std::snprintf(payload_buf, sizeof(payload_buf), "t%u-%u", t, i);
                std::string_view payload(payload_buf);

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

    const size_t expected = size_t(num_threads) * events_per_thread;
    if (ids.size() != expected) {
        std::cout << "LOST ENTRIES: " << ids.size() << " vs expected " << expected << "\n";
        return 1;
    }

    for (size_t i = 0; i < std::min<size_t>(1000, ids.size()); ++i) {
        auto [ok, val] = store.select(ids[i]);
        if (!ok || val.empty()) {
            std::cout << "CORRUPTED OR MISSING ID: " << ids[i] << "\n";
            return 1;
        }
    }

    std::cout << "Big Test Passed: " << ids.size() << " entries perfect\n";
    return 0;
}