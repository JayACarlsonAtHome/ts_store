//tests/ts_store_006/Test_006_XS.CPP

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/PersistCommon.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/EventSink.hpp"

// — Aggressive tail-reader stress test (500 threads × 100 ops)

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

// ———————————————————— Test configuration ————————————————————
constexpr size_t WRITER_THREADS     = 50;
constexpr size_t OPS_PER_THREAD     = 400;
constexpr size_t MAX_ENTRIES        = WRITER_THREADS * OPS_PER_THREAD;

alignas(64) inline std::atomic<size_t> log_stream_write_pos{0};
inline std::array<size_t, MAX_ENTRIES> log_stream_array{};
inline std::atomic<size_t> total_written{0};

using LogConfig = ts_store_config<false, 6, 20, 43, 9, 6, false>;
using LogxStore = ts_store<LogConfig>;


int main(int argc, char** argv) {
    auto _opts = jac::ts_store::inline_v001::parse_test_options(argc, argv);
    (void)_opts; // silence -Wunused
    LogxStore store(WRITER_THREADS, OPS_PER_THREAD);

    // Attach double-buffered (asynchronous) persistence for this test run.
    // Chosen via --persist binary|jtext (default jtext). --base-name can override the
    // output file prefix (runner uses this to place files under test_results/*/TS_STORE_TEST_.../).
    {
        std::string ptype = _opts.persist.empty() ? "jtext" : _opts.persist;
        std::string bname = _opts.base_name;
        if (bname.empty()) bname = "persist";

        std::unique_ptr<IEventSink> sink;
        const size_t im = LogConfig::the_IntMetrics;
        const size_t dm = LogConfig::the_DblMetrics;
        if (ptype == "binary") {
            sink = std::make_unique<BinaryEventSink>(bname, im, dm, PersistMode::All);
        } else {
            sink = std::make_unique<JTextEventSink>(bname, im, dm, PersistMode::All);
        }
        auto writer = std::make_unique<DoubleBufferedWriter>(std::move(sink), 10'000);
        store.attach_persistence(std::move(writer));
    }

    std::vector<std::thread> writers;
    writers.reserve(WRITER_THREADS);

    auto writer_start = steady_clock::now();
    std::cout << "Writer start time : "
              << duration_cast<microseconds>(writer_start.time_since_epoch()).count()
              << " µs\n";

    for (size_t t = 0; t < WRITER_THREADS; ++t) {
        writers.emplace_back([&, t]() {
            for (size_t i = 0; i < OPS_PER_THREAD; ++i) {
                // Now matches verify_test_payloads() expectations

                std::string payload ( LogxStore::test_messages[i % LogxStore::test_messages.size()]);
                std::string cat  = std::string( LogxStore::categories[t % LogxStore::categories.size()]);

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
                if (ok) {
                    size_t pos = log_stream_write_pos.fetch_add(1, std::memory_order_relaxed);
                    if (pos < MAX_ENTRIES) {
                        log_stream_array[pos] = id;
                    }
                    total_written.fetch_add(1, std::memory_order_release);
                }
            }
        });
    }

    while (total_written.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();

    auto reader_start = steady_clock::now();
    auto lag_us = duration_cast<microseconds>(reader_start - writer_start).count();
    std::cout << "Reader start time : "
              << duration_cast<microseconds>(reader_start.time_since_epoch()).count()
              << " µs\n";
    std::cout << "Reader start lag  : " << lag_us << " µs\n";

    std::atomic<long long> hits{0};
    std::atomic<long long> misses{0};

    std::thread tail_reader([&]() {
        size_t last_read = 0;

        while (total_written.load(std::memory_order_acquire) < WRITER_THREADS * OPS_PER_THREAD ||
               last_read < log_stream_write_pos.load(std::memory_order_acquire)) {

            size_t current_end = log_stream_write_pos.load(std::memory_order_acquire);

            while (last_read < current_end && last_read < MAX_ENTRIES) {
                uint64_t id = log_stream_array[last_read++];
                auto [ok, _] = store.select(id);
                (ok ? hits : misses).fetch_add(1, std::memory_order_relaxed);
            }

            if (last_read >= current_end)
                std::this_thread::sleep_for(microseconds(10));
        }
    });

    for (auto& w : writers) w.join();

    auto writer_stop = steady_clock::now();
    std::cout << "Writer stop time  : "
              << duration_cast<microseconds>(writer_stop.time_since_epoch()).count()
              << " µs\n";

    tail_reader.join();

    auto reader_stop = steady_clock::now();
    auto finish_lag = duration_cast<microseconds>(reader_stop - writer_stop).count();

    std::cout << "Reader stop time  : "
              << duration_cast<microseconds>(reader_stop.time_since_epoch()).count()
              << " µs\n";
    std::cout << "Reader finish lag : " << finish_lag << " µs\n\n";

    std::cout << "Tail-reader result: " << hits << " hits, " << misses << " misses (should be 0)\n";

    // Final verification
    if (!store.verify_level01()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        store.diagnose_failures();
        return 1;
    }

    if (!store.verify_level02()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        store.diagnose_failures();
        return 1;
    }

    std::cout << "ALL " << store.expected_size() <<" ENTRIES VERIFIED — ZERO CORRUPTION\n";
    store.show_duration("Store");
    store.print(0);
    return 0;
}
