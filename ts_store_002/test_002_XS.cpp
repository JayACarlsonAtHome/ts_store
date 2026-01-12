// test_002.cpp — 250,000 entry stress test — FINAL 2025 EDITION
// NOW 100% MATCHES FastPayload format: "thread:X event:Y payload-X-Y"

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <array>

using namespace jac::ts_store::inline_v001;

using LogConfig = ts_store_config<false>;  // BufferSize=96, TypeSize=12, CategorySize=24, UseTimestamps=true
using LogxStore = ts_store<LogConfig>;

constexpr std::array<std::string_view, 5> event_messages = {
    "[INFO] Processing request",
    "[WARN] Resource usage high",
    "[ERROR] Connection failed",
    "[INFO] Cache hit ratio 98%",
    "[DEBUG] Thread pool active"
};

int main() {
    constexpr uint32_t num_threads       = 250;
    constexpr uint32_t events_per_thread = 1000;
    constexpr uint64_t total_entries     = uint64_t(num_threads) * events_per_thread;

    std::cout << std::format("\033[1;31m=== ts_store {} entry stress test ===\033[0m\n", total_entries);
    std::cout << std::format("Threads: {}    Events/thread: {}    Total: {}\n\n",  num_threads, events_per_thread, total_entries);

    LogxStore store(num_threads, events_per_thread);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, &store] {
            for (uint32_t i = 0; i < events_per_thread; ++i) {
                // EXACT SAME FORMAT AS FastPayload — 100% compatible

                std::string_view extra = event_messages[t % event_messages.size()];
                std::string payload = store.generateTestPayload(t, i, extra);

                auto type     = std::string{"STRESS"};
                auto category = std::string{"PERF_TEST"};

                auto [ok, id] = store.save_event(t, payload, type, category);
                if (!ok) {
                    std::cout << std::format("\033[1;31m[FATAL] claim failed — thread {} event {}\033[0m\n", t, i);
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

    std::cout << std::format("╔════════════════════════════════════════════════╗\n"
                                    "║ All {:06} entries passed verification!  "
                                    "      ║\n"
                                    "╚════════════════════════════════════════════════╝\n",   total_entries);
    return 0;
}