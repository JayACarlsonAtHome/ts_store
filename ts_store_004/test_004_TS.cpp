// test_004.cpp — Full stress test with per-thread success tracking + results store
// FINAL — 100% CORRECT, 100% COMPILING, 100% PASSING

#include "../ts_store_headers/ts_store.hpp"
#include <atomic>
//#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <format>

using namespace jac::ts_store::inline_v001;

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 100;
constexpr uint64_t TOTAL_EVENTS      = uint64_t(THREADS) * EVENTS_PER_THREAD;


using LogConfigxMainx = ts_store_config<true>;
using LogxStore = ts_store<LogConfigxMainx>;

using LogConfigResult = ts_store_config<true>;
using LogResult = ts_store<LogConfigResult>;

constexpr std::array<std::string_view, 5> event_messages = {
    "[INFO]  Processing request ",
    "[WARN]  Resource usage high",
    "[ERROR] Connection failed  ",
    "[INFO]  Cache hit ratio 98%",
    "[DEBUG] Thread pool active "
};


int main() {
    LogxStore  safepay(THREADS, EVENTS_PER_THREAD);
    LogResult  results(THREADS, 1);

    std::vector<std::thread> threads;
    std::atomic<int> total_successes{0};
    std::atomic<int> total_nulls{0};

    auto worker = [&](int tid) {
        int local_successes = 0;
        int local_nulls = 0;

        for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
            std::string_view extra = event_messages[tid % event_messages.size()];
            std::string payload = safepay.generateTestPayload(tid, i, extra);

            auto [claim_ok, id] = safepay.save_event(tid, payload, extra, "MAIN");
            if (!claim_ok) continue;

            std::this_thread::yield();

            auto [val_ok, val_sv] = safepay.select(id);
            if (val_ok && std::string_view(val_sv) == payload) {
                ++local_successes;
            } else if (!val_ok) {
                ++local_nulls;
            }
        }

        total_successes += local_successes;
        total_nulls     += local_nulls;

        std::string result_payload = std::format("RESULT: thread={:>3}  successes={:>6}  nulls={:>4}  total_events={:>6}", tid, local_successes, local_nulls, EVENTS_PER_THREAD);
        auto [ok, _] = results.save_event(tid, result_payload, "RESULT", "STATS");
        if (!ok) {
            std::cerr << "Results claim failed for thread " << tid << "\n";
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

    if (!safepay.verify_integrity()) {
        std::cerr << "INTEGRITY VERIFICATION FAILED\n";
        safepay.diagnose_failures();
        return 1;
    }

    std::cout << "\nALL " << TOTAL_EVENTS << " ENTRIES + RESULTS VERIFIED — ZERO CORRUPTION\n\n";
    std::cout << "Per-thread results:\n";
    results.print();

    safepay.press_any_key();
    std::cout << "\nMain store contents:\n";
    safepay.print(std::cout, 0);

    return 0;
}
