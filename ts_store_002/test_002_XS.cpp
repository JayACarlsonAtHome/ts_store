// test_002.cpp — 250,000 entry stress test — FINAL 2025 EDITION
// NOW 100% MATCHES FastPayload format: "thread:X event:Y payload-X-Y"

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>
#include <thread>
#include <vector>
//#include "../fmt/include/fmt/core.h"
#include "../fmt/include/fmt/color.h"

using namespace jac::ts_store::inline_v001;

using BigStore = ts_store<

    fixed_string<512>,
    fixed_string<32>,
    fixed_string<64>,
    512, 32, 64,
    false        // UseTimestamps
>;

int main() {
    constexpr uint32_t num_threads       = 250;
    constexpr uint32_t events_per_thread = 1000;
    constexpr uint64_t total_entries     = uint64_t(num_threads) * events_per_thread;

    std::cout << fmt::format(fg(fmt::color::lime) | fmt::emphasis::bold,
        "=== ts_store {} entry stress test ===\n", total_entries);
    std::cout << fmt::format("Threads: {}    Events/thread: {}    Total: {}\n\n",
                             num_threads, events_per_thread, total_entries);

    BigStore store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store] {
            for (uint32_t i = 0; i < events_per_thread; ++i) {
                // EXACT SAME FORMAT AS FastPayload — 100% compatible
                auto payload = store.make_test_payload(t,i);
                auto type     = fixed_string<32>{"STRESS"};
                auto category = fixed_string<64>{"PERF_TEST"};

                auto [ok, id] = store.save_event(t, payload, type, category);
                if (!ok) {
                    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold,
                               "[FATAL] claim failed — thread {} event {}\n", t, i);
                    std::abort();
                }
            }
        });
    }

    std::cout << "All threads launched — crossing the streams at full power...\n";
    for (auto& th : threads) th.join();

    std::cout << "\nAll threads joined — running final verification...\n\n";

    if (!store.verify_integrity()) {
        std::cerr << "Structural verification failed!\n";
        store.diagnose_failures();
        return 1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_test_payloads()) {
        std::cerr << "Payload verification failed!\n";
        store.diagnose_failures();
        return 1;
    }
#endif

    fmt::print(fg(fmt::color::lime) | fmt::emphasis::bold,
        "╔════════════════════════════════════════════════╗\n"
               "║ All {:>12} entries passed verification!  ║\n"
               "╚════════════════════════════════════════════════╝\n",
        total_entries);

    return 0;
}