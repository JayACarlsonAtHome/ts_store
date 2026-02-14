//ts_store_002/Test_002_TS.CPP

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"

using namespace jac::ts_store::inline_v001;

using LogConfig = ts_store_config<true>;
using LogxStore = ts_store<LogConfig>;

int main() {
    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    constexpr size_t num_threads       = 25;
    constexpr size_t events_per_thread = 100;
    constexpr size_t total_entries     = uint64_t(num_threads) * events_per_thread;

    std::cout << ansi::yellow << std::format( "=== ts_store Test 002 TS with {} entries ===\n", total_entries) << ansi::reset;
    std::cout << ansi::white  << std::format("Threads: {}    Events/thread: {}    Total: {}\n\n",  num_threads, events_per_thread, total_entries) << ansi::reset;

    LogxStore store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store ] {
            for (size_t i = 0; i < events_per_thread; ++i) {

                std::string payload ( LogxStore::test_messages[i % LogxStore::test_messages.size()]);
                std::string cat  = std::string( LogxStore::categories[t % LogxStore::categories.size()]);

                uint64_t raw_flags = 0;
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::LogConsole);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::KeeperRecord);
                raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::HotCacheHint);
                raw_flags = set_severity(raw_flags, static_cast<TsStoreFlags::Severity>(i % 8));

                bool is_debug = true;
                auto [ok, id] = store.save_event(t, i, std::move(payload), raw_flags, std::move(cat), is_debug);
                if (!ok) {
                    std::cout << ansi::bold << ansi::red << std::format("[FATAL] claim failed — thread {} event {}\n", t, i) << ansi::reset;
                    std::abort();
                }
            }
        });
    }

    std::cout << "All threads launched — crossing the streams at full power...\n";
    for (auto& th : threads) th.join();

    std::cout << "\nAll threads joined — running final verification...\n\n";

    if (!store.verify_level01()) {
        std::cerr << ansi::bold << ansi::red << "Structural verification failed!\n" << ansi::reset;
        store.diagnose_failures();
        return 1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_level02()) {
        std::cerr <<ansi::bold << ansi::red << "Payload verification failed!\n" << ansi::reset;
        store.diagnose_failures();
        return 1;
    }
#endif

    std::cout   << ansi::bold << ansi::blue
                << std::format("╔════════════════════════════════════════════════╗\n"
                                    "║ All {:06} entries passed verification!        ║\n"
                                    "╚════════════════════════════════════════════════╝\n",   total_entries)
                << ansi::reset;
    return 0;
}