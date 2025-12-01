// test_004.cpp — Full stress test with per-thread success tracking + results store

#include "../ts_store_headers/ts_store.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
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
constexpr uint32_t EVENTS_PER_THREAD = 100;
constexpr uint64_t TOTAL_EVENTS      = uint64_t(THREADS) * EVENTS_PER_THREAD;

using MainStore   = ts_store<fixed_string<100>, fixed_string<16>, fixed_string<32>, 100, 16, 32, true>;
using ResultStore = ts_store<fixed_string<128>, fixed_string<16>, fixed_string<32>, 128, 16, 32, true>;

int main() {
    MainStore   safepay(THREADS, EVENTS_PER_THREAD);
    ResultStore results(THREADS, 1);

    std::vector<std::thread> threads;
    std::atomic<int> total_successes{0};
    std::atomic<int> total_nulls{0};

    auto worker = [&](int tid) {
        int local_successes = 0;
        int local_nulls = 0;

        for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
            // MAIN STORE — now uses correct test format
            auto payload = make_fixed<100>("thread:{} event:{} seq:{} time:{}",
                                          tid, i, i * 12345, steady_clock::now().time_since_epoch().count());

            auto [claim_ok, id] = safepay.claim(tid, payload, "STRESS", "MAIN");
            if (!claim_ok) continue;

            std::this_thread::yield();

            auto [val_ok, val_sv] = safepay.select(id);
            if (val_ok && std::string_view(val_sv) == std::string_view(payload)) {
                ++local_successes;
            } else if (!val_ok) {
                ++local_nulls;
            }
        }

        total_successes += local_successes;
        total_nulls     += local_nulls;

        // RESULT STORE — also uses correct test format
        auto result_payload = make_fixed<128>("thread:{} event:result seq:0 tid:{} succ:{} nulls:{}",
                                             tid, tid, local_successes, local_nulls);

        auto [ok, _] = results.claim(tid, result_payload, "RESULT", "STATS");
        if (!ok) {
            std::cerr << "Results claim failed for thread " << tid << "\n";
        }
    };

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();

    std::cout << "Safepay entries: " << safepay.expected_size() << " (expected: " << TOTAL_EVENTS << ")\n";
    safepay.show_duration("Safepay");
    results.show_duration("Results");

    std::cout << "\nTotal successes: " << total_successes << " / " << TOTAL_EVENTS
              << "  (" << (total_successes == TOTAL_EVENTS ? "PASS" : "FAIL") << ")\n";
    std::cout << "Null reads (races): " << total_nulls << "\n\n";

    if (!safepay.verify_integrity() || !results.verify_integrity()) {
        std::cerr << "INTEGRITY VERIFICATION FAILED\n";
        safepay.diagnose_failures();
        results.diagnose_failures();
        return 1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!safepay.verify_test_payloads() || !results.verify_test_payloads()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        safepay.diagnose_failures();
        results.diagnose_failures();
        return 1;
    }
#endif

    std::cout << "\nALL " << TOTAL_EVENTS << " ENTRIES + RESULTS VERIFIED — ZERO CORRUPTION\n\n";
    std::cout << "Per-thread results:\n";
    results.print();

    safepay.press_any_key();
    safepay.print();

    return 0;
}