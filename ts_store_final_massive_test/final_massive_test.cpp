//
// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
// 10-run benchmark using store.clear() — fastest, most realistic, unbreakable
//
#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 4000;
constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;

constexpr size_t BUFFER_SIZE    = 100;
constexpr size_t TYPE_SIZE      = 16;
constexpr size_t CATEGORY_SIZE  = 32;
constexpr bool   USE_TIMESTAMPS = true;

int run_single_test(ts_store<BUFFER_SIZE, TYPE_SIZE, CATEGORY_SIZE, USE_TIMESTAMPS>& store)
{
    store.clear();  // Fresh start — zero cost, perfect reset

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &store]() {
            for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
                auto payload = FastPayload<BUFFER_SIZE>::make(t, i);
                auto [ok, id] = store.claim(t, payload, "MASSIVE", "FINAL");
                if (!ok || id >= (1ull << 50)) {
                    std::cerr << "CLAIM FAILED\n";
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto mid = high_resolution_clock::now();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    if (ids.size() != TOTAL) {
        std::cout << "LOST " << TOTAL - ids.size() << " ENTRIES\n";
        return -1;
    }

    // 100% verification — NOT in write timing
    //std::cout << "Verifying all " << TOTAL << " entries...\n";
    size_t errors = 0;
    for (uint64_t id : ids) {
        auto [ok, val] = store.select(id);
        if (!ok || val.find("payload-") != 0) {
            ++errors;
            if (errors <= 10) std::cout << "  Corruption at ID " << id << "\n";
        }
    }

    if (errors > 0) {
        std::cout << "FAILED: " << errors << " corrupted entries:  ";
        return -1;
    }

    std::cout << "100% correct:  ";

    auto write_us = duration_cast<microseconds>(mid - start).count();
    return static_cast<int>(write_us);
}

int main()
{
    constexpr int RUNS = 50;

    // One store — memory guard runs ONCE
    ts_store<BUFFER_SIZE, TYPE_SIZE, CATEGORY_SIZE, USE_TIMESTAMPS> store(
        THREADS, EVENTS_PER_THREAD);

    long long total_write_us = 0;
    int failed_runs = 0;

    std::cout << "Running massive test — " << RUNS << " iterations (store.clear() between runs)\n";
    std::cout << "Benchmark measures PURE WRITE throughput — verification is separate and untimed\n\n";

    for (int run = 1; run <= RUNS; ++run) {
        std::cout << "Run " << std::right << std::setw(2) << run
                  << " of " << RUNS << ":  ";

        int result = run_single_test(store);

        if (result < 0) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else {
            total_write_us += result;
            double ops_per_sec = 1'000'000.0 * 1'000'000.0 / result;

            std::cout << "PASS — "
                      << std::setw(7) << result << " µs → "
                      << std::fixed << std::setprecision(0)
                      << std::setw(10) << static_cast<uint64_t>(ops_per_sec + 0.5)
                      << " ops/sec\n";
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed.\n";
        return 1;
    }

    double avg_us = static_cast<double>(total_write_us) / RUNS;
    double avg_ops_sec = 1'000'000.0 * 1'000'000.0 / avg_us;

    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "            FINAL RESULT — " << RUNS << "-RUN AVERAGE          \n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Average write time : " << std::setw(9) << avg_us << " µs\n";
    std::cout << "  Average throughput : " << std::setw(10)
              << static_cast<uint64_t>(avg_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  (1,000,000 events per run, 100% verified)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}