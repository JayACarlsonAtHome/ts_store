//ts_store_004/Test_004_XS.CPP

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"

using namespace jac::ts_store::inline_v001;

constexpr size_t THREADS           = 250;
constexpr size_t EVENTS_PER_THREAD = 400;
constexpr size_t TOTAL_EVENTS      = size_t(THREADS) * EVENTS_PER_THREAD;


using LogConfigxMainx = ts_store_config<false>;
using LogxStore = ts_store<LogConfigxMainx>;

using LogConfigResult = ts_store_config<false>;
using LogResult = ts_store<LogConfigResult>;

int main() {
    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    LogxStore  safepay(THREADS, EVENTS_PER_THREAD);
    LogResult  results(THREADS, 1);

    std::vector<std::thread> threads;
    std::atomic<size_t> total_successes{0};
    std::atomic<size_t> total_nulls{0};

    auto worker = [&](size_t t) {
        size_t local_successes = 0;
        size_t local_nulls = 0;

        for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {

            std::string payload ( LogxStore::test_messages[i % LogxStore::test_messages.size()]);
            std::string_view payload_copy = payload;
            std::string cat  = std::string( LogxStore::categories[t % LogxStore::categories.size()]);

            uint64_t raw_flags = 0;
            raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::LogConsole);
            raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::KeeperRecord);
            raw_flags = set_severity(raw_flags, static_cast<TsStoreFlags::Severity>(i % 8));

            bool is_debug = true;
            auto [ok, id] = safepay.save_event(t, i, std::move(payload), raw_flags, std::move(cat), is_debug);
            auto [val_ok, val_sv] = safepay.select(id);
            if (val_ok && std::string_view(val_sv) == payload_copy) {
                ++local_successes;
            } else if (!val_ok) {
                ++local_nulls;
            }
        }

        total_successes += local_successes;
        total_nulls     += local_nulls;

        std::string result_payload = std::format("RESULT: thread={:>3}  successes={:>6}  nulls={:>4}  total_events={:>6}", t, local_successes, local_nulls, EVENTS_PER_THREAD);

        uint64_t raw_flags = 0;
        raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::LogConsole);
        raw_flags = set_user_flag(raw_flags, TsStoreFlags::UserFlag::KeeperRecord);
        raw_flags = set_severity(raw_flags, static_cast<TsStoreFlags::Severity>(TsStoreFlags::Severity::Info));

        auto [ok, _] = results.save_event(t, local_successes, std::move(result_payload), raw_flags, "STATS");
        if (!ok) {
            std::cerr << "Results claim failed for thread " << t << "\n";
        }
    };

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();

    std::cout << "Safepay entries: " << safepay.expected_size()
              << " (expected: " << TOTAL_EVENTS << ")\n";
    safepay.show_duration("Safepay");
    results.show_duration("Results");

    std::cout << "\nTotal successes: " << total_successes << " / " << TOTAL_EVENTS
              << "  (" << (total_successes == TOTAL_EVENTS ? "PASS" : "FAIL") << ")\n";
    std::cout << "Null reads (races): " << total_nulls << "\n\n";

    if (!safepay.verify_level01()) {
        std::cerr << "INTEGRITY VERIFICATION FAILED\n";
        safepay.diagnose_failures();
        return 1;
    }

    //results.debug_print_widths();
    std::cout << "\nResult store contents:\n";
    results.print();
    std::cout << "\nALL " << TOTAL_EVENTS << " ENTRIES + RESULTS VERIFIED — ZERO CORRUPTION\n\n";
    safepay.press_any_key();

    //safepay.debug_print_widths();
    std::cout << "Per-thread results:\n";
    safepay.print( 0);
    return 0;
}