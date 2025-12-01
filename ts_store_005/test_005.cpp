// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
// 10-run benchmark using store.clear() — fastest, most realistic, unbreakable

#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <format>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

template<size_t N, class... Args>
constexpr fixed_string<N> make_fixed(std::format_string<Args...> fmt, Args&&... args) {
    std::string temp = std::format(fmt, std::forward<Args>(args)...);
    fixed_string<N> result;
    const size_t copy_len = std::min(temp.size(), N - 1);
    std::memcpy(result.data, temp.data(), copy_len);
    result.data[copy_len] = '\0';
    return result;
}

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 4000;
constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;

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
                auto payload = make_fixed<100>("thread:{} event:{} seq:{} time:{}",
                                              t, i, i * 12345, steady_clock::now().time_since_epoch().count());

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

    // Full verification — separate from write timing
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
    constexpr int RUNS = 50;

    MassiveStore store(THREADS, EVENTS_PER_THREAD);

    long long total_write_us = 0;
    int failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — 1,000,000 entries × " << RUNS << " runs ===\n";
    std::cout << "Using store.clear() — fastest, most realistic reuse\n\n";

    for (int run = 1; run <= RUNS; ++run) {
        std::cout << "Run " << std::setw(2) << run << " / " << RUNS << ": ";

        int result = run_single_test(store);

        if (result < 0) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else {
            total_write_us += result;
            double ops_per_sec = 1'000'000.0 * 1'000'000.0 / result;

            std::cout << "PASS — "
                      << std::setw(8) << result << " µs → "
                      << std::fixed << std::setprecision(0)
                      << std::setw(9) << static_cast<uint64_t>(ops_per_sec + 0.5)
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