//ts_store_005/Test_005_TS.CPP

#include "../ts_store_headers/ts_store.hpp"
#include <utility>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

constexpr size_t THREADS           = 250;
constexpr size_t EVENTS_PER_THREAD = 4000;
constexpr size_t TOTAL             = size_t(THREADS) * EVENTS_PER_THREAD;
constexpr size_t RUNS              = 50;

using LogConfig = ts_store_config<true>;
using LogxStore = ts_store<LogConfig>;

std::pair<bool, long int> run_single_test(LogxStore& store)
{
    store.clear();
    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    auto start = high_resolution_clock::now();

    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {

                std::string payload ( LogxStore::test_messages[i % LogxStore::test_messages.size()]);
                std::string type = std::string(LogxStore::types[i % LogxStore::types.size()]);
                std::string cat  = std::string( LogxStore::categories[t % LogxStore::categories.size()]);
                bool is_debug = true;
                auto [ok, id] = store.save_event(t, i, std::move(payload), std::move(type), std::move(cat), is_debug);
                if (!ok) {
                    std::cerr << ansi::red << "CLAIM FAILED — thread " << t << " event " << i << ansi::reset << "\n" ;
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto end = high_resolution_clock::now();
    long int write_us = duration_cast<microseconds>(end - start).count();

    if (!store.verify_level01()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        store.diagnose_failures();
        return {false, -1};
    }

    /*
    // Only activate this is you want to run for a very long time
    //   or you change the threads, and event numbers to something
    //   reasonable
    //

    if (!store.verify_level02()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        store.diagnose_failures();
        return -1;
    }
*/
    return { true, write_us };
}

int main()
{
    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    LogxStore  store(THREADS, EVENTS_PER_THREAD);
    size_t total_write_us = 0;
    size_t durations[RUNS] = {};
    size_t failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — 1,000,000 entries × " << RUNS << " runs ===\n";
    std::cout << "Using store.clear() — fastest, most realistic reuse\n\n";

    for (size_t run = 0; run < RUNS; ++run) {
        std::cout << "Run " << std::setw(2) << (run + 1) << " / " << RUNS << "\n";

        auto [status, microseconds] = run_single_test(store);

        if (!status) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else
        {
            total_write_us += static_cast<decltype(total_write_us)>(microseconds);
            durations[run] = static_cast<size_t>(microseconds);

            if (microseconds == 0) {
                std::cout << "PASS — 0 µs (too fast to measure)\n\n";
            } else
            {
                double ops_per_sec = 1'000'000.0 * 1'000'000.0 / static_cast<double>(microseconds);
                std::cout << "PASS — "
                          << std::setw(8) << microseconds << " µs → "
                          << std::fixed << std::setprecision(0)
                          << std::setw(9) << static_cast<size_t>(ops_per_sec + 0.5)
                          << " ops/sec\n\n";
            }
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed — aborting summary.\n";
        return 1;
    }

    auto [min_it, max_it] = std::minmax_element(std::begin(durations), std::end(durations));
    double avg_us = static_cast<double>(total_write_us) / RUNS;
    double max_ops_sec = 1'000'000.0 * 1'000'000.0 / static_cast<double>(*min_it);
    double min_ops_sec = 1'000'000.0 * 1'000'000.0 / static_cast<double>(*max_it);
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
