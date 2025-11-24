// ts_store_nuclear_test.cpp
#include "../ts_store.hpp" // Uses BufferSize=80 default, pair<bool,...> returns
#include <thread>
#include <vector>
#include <iostream>

int main() {
    ts_store<80> store(true);
    store.reserve(250000);

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

    auto ids = store.get_claimed_ids_sorted(0);
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

    std::cout << "NUCLEAR TEST PASSED: " << ids.size() << " entries perfect\n";
}