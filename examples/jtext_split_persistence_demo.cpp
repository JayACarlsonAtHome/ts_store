// examples/jtext_split_persistence_demo.cpp
// Demonstrates JTextSplitEventLog with:
// - 9 integer + 6 double metrics
// - Early header + file open (before any "processing")
// - 10K auto-batching enabled by default (max throughput)
// - KeeperRecord filtering option

#include <chrono>
#include <iostream>
#include <vector>
#include <random>

import jac.ts_store.persistence.jtext;

using namespace jac::ts_store::inline_v001;

int main() {
    constexpr size_t NUM_EVENTS = 50'000;   // decent size for metrics
    constexpr size_t INT_COUNT  = 9;
    constexpr size_t DBL_COUNT  = 6;

    std::cout << "=== ts_store jText Split Persistence Demo ===\n";
    std::cout << "Events: " << NUM_EVENTS << "\n";
    std::cout << "Metrics: " << INT_COUNT << " ints + " << DBL_COUNT << " doubles\n\n";

    auto start = std::chrono::steady_clock::now();

    // === EARLY OPEN + HEADERS (before any "work") ===
    JTextSplitEventLog log("Event_Demo", INT_COUNT, DBL_COUNT);

    auto header_done = std::chrono::steady_clock::now();
    auto header_us = std::chrono::duration_cast<std::chrono::microseconds>(header_done - start).count();

    std::cout << "Headers + files opened in " << header_us << " µs\n";
    std::cout << "(File paths are Event_Demo*.jtext)\n\n";

    // Simulate events
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> int_dist(-100000, 100000);
    std::uniform_real_distribution<double> dbl_dist(-1.0, 1.0);
    std::uniform_int_distribution<int> flag_dist(0, 7);

    std::vector<int64_t> ints(INT_COUNT);
    std::vector<double>  dbls(DBL_COUNT);

    auto work_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        for (auto& v : ints) v = int_dist(rng);
        for (auto& v : dbls) v = dbl_dist(rng);

        uint64_t flags = 0;
        if (flag_dist(rng) == 0) {
            // Occasionally set KeeperRecord (bit 1)
            flags |= (1ULL << 1);
        }

        log.append_event(
            i,                    // event_id (linking key)
            i % 250,              // thread_id
            i % 4000,             // per_thread_event_id
            flags,
            "DEMO",
            "payload for event",
            i * 1000,             // fake timestamp
            ints,
            dbls
        );
    }

    auto work_end = std::chrono::steady_clock::now();
    auto work_us = std::chrono::duration_cast<std::chrono::microseconds>(work_end - work_start).count();

    log.flush();   // explicit flush at end of "batch"

    auto total_end = std::chrono::steady_clock::now();
    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(total_end - start).count();

    const auto& s = log.stats();

    std::cout << "Appended " << NUM_EVENTS << " events in " << work_us << " µs\n";
    std::cout << "  → " << static_cast<double>(NUM_EVENTS) * 1'000'000.0 / static_cast<double>(work_us) << " events/sec (append path)\n";
    std::cout << "  Rows → Main: " << s.main_rows << " | Ints: " << s.ints_rows << " | Floats: " << s.floats_rows << "\n";
    std::cout << "  Batches flushed: " << s.batches_flushed << "\n";
    std::cout << "Total time (early open + work + flush): " << total_us << " µs\n\n";

    log.finalize();

    std::cout << "Demo complete. 10K auto-batching was active on all three writers.\n";
    std::cout << "Inspect the three .jtext files (they now have proper Field Lists).\n";

    return 0;
}
