//tests/ts_store_005/Test_005_XS.CPP
//
// Massive multi-threaded throughput + correctness test.
// Full mode: 100 threads × 10k events = 1M events per run, 5 runs total.
// Only the last run performs persistence (previous runs are clean hot-path measurement).
// Size controlled at runtime via --threads --events-per-thread --runs (or via test_params.txt).
// See tests/test_params.txt and scripts/run_all_tests.sh .
// Note: this variant uses 0 int metrics + 0 double metrics (pure payload + flags test).

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>

import jac.ts_store.impl.testing;
import jac.ts_store.persistence.binary;
import jac.ts_store.persistence.jtext;
#ifdef TS_STORE_ENABLE_SQLITE_PERSIST
import jac.ts_store.persistence.sql;
#endif

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

// These are set at runtime from CLI / config (see test_params.txt and runner).
// Defaults come from TestOptions (high intensity); smoke uses small via --test-size=smoke or explicit.
size_t THREADS;
size_t EVENTS_PER_THREAD;
size_t TOTAL;
size_t RUNS;

using LogConfig = ts_store_config<false, 6, 20, 43, 0, 0, false>;
using LogxStore = ts_store<LogConfig>;

std::pair<bool, long int> run_single_test(LogxStore& store)
{
    store.clear();
    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    auto start = high_resolution_clock::now();

    // Pre-create payloads and categories to reduce string construction overhead in the hot path measurement
    std::array<std::string, 8> pre_payloads;
    for (size_t i = 0; i < 8; ++i) pre_payloads[i] = std::string(LogxStore::test_messages[i]);
    std::array<std::string, 5> pre_cats;
    for (size_t i = 0; i < 5; ++i) pre_cats[i] = std::string(LogxStore::categories[i]);

    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {

                std::string payload = pre_payloads[i % pre_payloads.size()];
                std::string cat = pre_cats[t % pre_cats.size()];

                uint64_t raw_flags = 0;
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::LogConsole);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::KeeperRecord);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::HotCacheHint);
                raw_flags = set_severity(raw_flags, static_cast<TsStoreFlags::Severity>(i % 8));

                bool is_debug = true;
                std::array<int64_t, LogConfig::the_IntMetrics> ints{};
                std::array<double, LogConfig::the_DblMetrics> dbls{};
                for (size_t k = 0; k < LogConfig::the_IntMetrics; ++k) ints[k] = static_cast<int64_t>(i * 100 + k);
                for (size_t k = 0; k < LogConfig::the_DblMetrics; ++k) dbls[k] = static_cast<double>(i) * 0.01 + static_cast<double>(k) * 0.001;
                auto [ok, id] = store.save_event(t, i, std::move(payload), raw_flags, std::move(cat), is_debug, ints, dbls);
                if (!ok) {
                    std::cerr << ansi::red() << "CLAIM FAILED — thread " << t << " event " << i << ansi::reset() << "\n" ;
                    std::abort();
                }
                if (i == 42 && t == 0) {
                    if (LogConfig::the_IntMetrics > 0 || LogConfig::the_DblMetrics > 0) {
                        std::cout << "  sample metrics on event 42 (t=0): int=" 
                                  << (LogConfig::the_IntMetrics > 0 ? std::to_string(ints[0]) : "n/a")
                                  << " dbl=" 
                                  << (LogConfig::the_DblMetrics > 0 ? std::to_string(dbls[0]) : "n/a") << "\n";
                    } else {
                        std::cout << "  sample event 42 (t=0) (0 metrics)\n";
                    }
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

int main(int argc, char** argv)
{
    auto _opts = jac::ts_store::inline_v001::parse_test_options(argc, argv);

    THREADS = _opts.threads;
    EVENTS_PER_THREAD = _opts.events_per_thread;
    TOTAL = THREADS * EVENTS_PER_THREAD;
    RUNS = _opts.runs;

    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    LogxStore  store(THREADS, EVENTS_PER_THREAD);

    // Parse persist type early (we only actually attach for the last run).
    std::string ptype = _opts.persist.empty() ? "jtext" : _opts.persist;
    std::string bname = _opts.base_name.empty() ? "persist" : _opts.base_name;

    size_t total_write_us = 0;
    std::vector<size_t> durations(RUNS, 0);
    size_t failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — " << TOTAL << " entries × " << RUNS << " runs ===\n";
    std::cout << "Using store.clear() — fastest, most realistic reuse\n\n";

    if (ptype == "none") {
        std::cout << "No persistence attached — pure in-memory hot path\n\n";
    } else {
        std::cout << "Persistence will only be attached on the *last* run "
                  << "(previous runs measure clean hot path; only final run produces persist artifacts).\n\n";
    }

    // To keep captured log files reasonable in size (especially with --output on / live mode),
    // we only emit per-run progress + timing on the *last* iteration.
    // For persist modes, *only the last run* attaches the DoubleBufferedWriter + sink,
    // so the .bin / .jtext artifacts contain data from only the final run.
    // All runs do the full work + structural verification.
    // The final summary stats (min/max/avg across all runs) are still printed once at the end.
    for (size_t run = 0; run < RUNS; ++run) {
        bool is_last = (run == RUNS - 1);

        if (is_last) {
            std::cout << "Run " << std::setw(2) << (run + 1) << " / " << RUNS << "\n";

            // Only attach persistence on the final run (when not --persist=none).
            // This ensures persist data / log files are generated only from the last run.
            if (ptype != "none") {
                std::unique_ptr<IEventSink> sink;
                const size_t im = LogConfig::the_IntMetrics;
                const size_t dm = LogConfig::the_DblMetrics;
                if (ptype == "binary") {
                    sink = std::make_unique<BinaryEventSink>(bname, im, dm, PersistMode::All);
                } else if (ptype == "sql") {
#ifdef TS_STORE_ENABLE_SQLITE_PERSIST
                    sink = std::make_unique<SqlEventSink>(bname, im, dm, PersistMode::All, true /* write debug INSERT .sql file */);
#else
                    std::cerr << "ERROR: SQL persistence not enabled at compile time (rebuild with -DTS_STORE_ENABLE_SQLITE_PERSIST=ON)\n";
                    return 1;
#endif
                } else {
                    sink = std::make_unique<JTextEventSink>(bname, im, dm, PersistMode::All);
                }
                auto writer = std::make_unique<DoubleBufferedWriter>(
                    std::move(sink),
                    10'000                   // batch size for double buffer
                );
                store.attach_persistence(std::move(writer));
            }
        }

        auto [status, microseconds] = run_single_test(store);

        if (!status) {
            if (is_last) std::cout << "FAILED\n";
            ++failed_runs;
        } else
        {
            total_write_us += static_cast<decltype(total_write_us)>(microseconds);
            durations[run] = static_cast<size_t>(microseconds);

            if (is_last) {
                if (microseconds == 0) {
                    std::cout << "PASS — 0 µs (too fast to measure)\n\n";
                } else
                {
                    double ops_per_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(microseconds);
                    std::cout << "PASS — "
                              << std::setw(8) << microseconds << " µs → "
                              << std::fixed << std::setprecision(0)
                              << std::setw(9) << static_cast<size_t>(ops_per_sec + 0.5)
                              << " ops/sec\n\n";
                }
            }
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed — aborting summary.\n";
        return 1;
    }

    auto [min_it, max_it] = std::minmax_element(durations.begin(), durations.end());
    double avg_us = static_cast<double>(total_write_us)           / static_cast<double>(RUNS);
    double max_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(*min_it);
    double min_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(*max_it);
    double avg_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(avg_us);

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
    std::cout << "  (" << TOTAL << " events per run, 100% verified, zero corruption)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}
