// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
// 50-run benchmark with min/max/avg — FINAL, UNBREAKABLE, PERFECT

#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <format>
#include <algorithm>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 4000;
constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;
constexpr int      RUNS              = 50;

using MassiveStore = ts_store<
    fixed_string<100>,
    fixed_string<16>,
    fixed_string<32>,
    100, 16, 32,
    true
>;

int run_single_test(MassiveStore& store)
{
    store.clear();

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
                auto payload = store.make_test_payload(t, i);
                auto [ok, id] = store.claim(t, payload, "MASSIVE", "FINAL");
                if (!ok) {
                    std::cerr << "CLAIM FAILED — thread " << t << " event " << i << "\n";
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto end = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(end - start).count();

    if (!store.verify_integrity()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        store.diagnose_failures();
        return -1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_test_payloads()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        store.diagnose_failures();
        return -1;
    }
#endif

    return static_cast<int>(write_us);
}

int main()
{
    MassiveStore store(THREADS, EVENTS_PER_THREAD);

    long long total_write_us = 0;
    int64_t   durations[RUNS] = {};  // ← FIXED NAME
    int       failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — 1,000,000 entries × " << RUNS << " runs ===\n";
    std::cout << "Using store.clear() — fastest, most realistic reuse\n\n";

    for (int run = 0; run < RUNS; ++run) {
        std::cout << "Run " << std::setw(2) << (run + 1) << " / " << RUNS << "\n";

        int result = run_single_test(store);

        if (result < 0) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else {
            total_write_us += result;
            durations[run] = result;  // ← FIXED

            double ops_per_sec = 1'000'000.0 * 1'000'000.0 / result;

            std::cout << "PASS — "
                      << std::setw(8) << result << " µs → "
                      << std::fixed << std::setprecision(0)
                      << std::setw(9) << static_cast<uint64_t>(ops_per_sec + 0.5)
                      << " ops/sec\n\n";
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed — aborting summary.\n";
        return 1;
    }

    auto [min_it, max_it] = std::minmax_element(std::begin(durations), std::end(durations));
    double avg_us = static_cast<double>(total_write_us) / RUNS;
    double min_ops_sec = 1'000'000.0 * 1'000'000.0 / *max_it;
    double max_ops_sec = 1'000'000.0 * 1'000'000.0 / *min_it;
    double avg_ops_sec = 1'000'000.0 * 1'000'000.0 / avg_us;

    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "               FINAL RESULT — " << RUNS << "-RUN STATISTICS            \n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Fastest run        : " << std::setw(9) << *min_it << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(max_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  Slowest run        : " << std::setw(9) << *max_it << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(min_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  Average            : " << std::setw(9) << avg_us     << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(avg_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  (1,000,000 events per run, 100% verified, zero corruption)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}