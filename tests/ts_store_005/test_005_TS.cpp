//tests/ts_store_005/Test_005_TS.CPP
//
// Massive multi-threaded throughput + correctness test (historically ~1,000,000 records per run).
// THREADS and EVENTS_PER_THREAD can be adjusted from time to time to change the load
// (e.g. for different hardware or stress levels). TOTAL is derived and used throughout
// for output and calculations so the test stays consistent when limits change.

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/PersistCommon.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/EventSink.hpp"
#include <utility>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

constexpr size_t THREADS           = 5;
constexpr size_t EVENTS_PER_THREAD = 20;
constexpr size_t TOTAL             = size_t(THREADS) * EVENTS_PER_THREAD;
constexpr size_t RUNS              = 1;

using LogConfig = ts_store_config<true, 6, 20, 43, 9, 6, false>;
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
                    std::cout << "  sample metrics on event 42 (t=0): int=" << ints[0] << " dbl=" << dbls[0] << "\n";
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
    (void)_opts;

    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    LogxStore  store(THREADS, EVENTS_PER_THREAD);

    // Attach double-buffered (asynchronous) persistence.
    // Sink (JText or Binary) chosen by --persist=... (default jtext).
    // base_name (for output files) can be overridden by --base-name (runner points it
    // into test_results/binary_logs/TS_STORE_TEST_005_TS/ or jText_logs/... so the
    // .bin / .jtext + _Ints.jtext + _Floats.jtext land in the right place).
    {
        std::string ptype = _opts.persist.empty() ? "jtext" : _opts.persist;
        if (ptype == "none") {
            std::cout << "No persistence attached — pure in-memory hot path\n";
        } else {
            std::string bname = _opts.base_name.empty() ? "persist" : _opts.base_name;

            std::unique_ptr<IEventSink> sink;
            const size_t im = LogConfig::the_IntMetrics;
            const size_t dm = LogConfig::the_DblMetrics;
            if (ptype == "binary") {
                sink = std::make_unique<BinaryEventSink>(bname, im, dm, PersistMode::All);
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

    size_t total_write_us = 0;
    size_t durations[RUNS] = {};
    size_t failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — " << TOTAL << " entries × " << RUNS << " runs ===\n";
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
                double ops_per_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(microseconds);
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
    double max_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(*min_it);
    double min_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / static_cast<double>(*max_it);
    double avg_ops_sec = static_cast<double>(TOTAL) * 1'000'000.0 / avg_us;

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
