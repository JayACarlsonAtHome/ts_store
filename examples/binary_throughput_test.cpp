// examples/binary_throughput_test.cpp
// Dedicated high-throughput test for the fast binary path.

#include <beman/ts_store/ts_store_headers/persistence/BinaryEventLog.hpp>

#include <chrono>
#include <iostream>
#include <vector>
#include <random>

using namespace jac::ts_store::inline_v001;

int main() {
    constexpr size_t NUM_EVENTS = 500'000;
    constexpr size_t INT_COUNT  = 9;
    constexpr size_t DBL_COUNT  = 6;

    std::cout << "=== BinaryEventLog High-Throughput Test ===\n";
    std::cout << "Events: " << NUM_EVENTS << " | " << INT_COUNT << " ints + " << DBL_COUNT << " doubles\n\n";

    BinaryEventLog log("Binary_Throughput_Test", INT_COUNT, DBL_COUNT, PersistMode::All, 32 * 1024 * 1024); // 32MB buffer

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> int_dist(-100000, 100000);
    std::uniform_real_distribution<double> dbl_dist(-1000.0, 1000.0);

    std::vector<int64_t> ints(INT_COUNT);
    std::vector<double>  dbls(DBL_COUNT);

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        for (auto& v : ints) v = int_dist(rng);
        for (auto& v : dbls) v = dbl_dist(rng);

        uint64_t flags = (i % 23 == 0) ? (1ULL << 1) : 0;

        log.append_event(i, i % 250, i % 4000, flags,
                         "THROUGHPUT", "binary test payload", i * 777,
                         ints, dbls);
    }

    log.flush();

    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double eps = static_cast<double>(NUM_EVENTS) * 1'000'000.0 / static_cast<double>(us);

    std::cout << "Appended " << NUM_EVENTS << " events in " << us << " µs\n";
    std::cout << "Throughput: " << static_cast<long long>(eps) << " events/sec\n";
    std::cout << "File: " << log.file_path() << "\n";

    log.finalize();
    return 0;
}
