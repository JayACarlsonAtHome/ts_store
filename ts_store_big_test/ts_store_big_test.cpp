// ts_store_big_test.cpp
#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>

int main() {
    ts_store<1000,1000, 80, true> store;

    std::vector<std::thread> threads;
    constexpr int N = 250;
    constexpr int OPS = 1000;

    for (int t = 0; t < N; ++t) {
        threads.emplace_back([t, &store]() {
            for (int i = 0; i < OPS; ++i) {
                std::string s = "t" + std::to_string(t) + "-" + std::to_string(i);
                auto [ok, id] = store.claim(t, s);
                if (!ok) std::cerr << "CLAIM FAILED\n";
            }
        });
    }
    for (auto& th : threads) th.join();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    if (ids.size() != N * OPS) {
        std::cout << "LOST ENTRIES: " << ids.size() << " vs " << N*OPS << "\n";
        return 1;
    }

    for (auto id : ids) {
        auto [ok, val] = store.select(id);
        if (!ok || val.empty()) {
            std::cout << "CORRUPTED OR MISSING ID: " << id << "\n";
            return 1;
        }
    }

    std::cout << "Big Test Passed: " << ids.size() << " entries perfect\n";
}