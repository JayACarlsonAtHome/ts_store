// examples/double_buffered_persistence_demo.cpp
//
// Demonstrates the new pluggable double-buffered persistence:
// - Same DoubleBufferedWriter works with JTextEventSink or BinaryEventSink
// - Hot path (submit_event) stays fast; draining happens in background thread
// - Easy to plug in future SQL sinks the same way

#include <beman/ts_store/ts_store_headers/persistence/EventSink.hpp>
#include <beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp>
#include <beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp>
#include <beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace jac::ts_store::inline_v001;

static void run_with_sink(const std::string& name, std::unique_ptr<IEventSink> sink) {
    constexpr size_t NUM_EVENTS = 100'000;
    constexpr size_t INT_COUNT  = 9;
    constexpr size_t DBL_COUNT  = 6;

    std::cout << "\n=== DoubleBufferedWriter + " << name << " ===\n";
    std::cout << "Events: " << NUM_EVENTS << " | " << INT_COUNT << " ints + " << DBL_COUNT << " doubles\n";

    DoubleBufferedWriter writer(std::move(sink), 10'000);  // 10k batch size

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> int_dist(-100000, 100000);
    std::uniform_real_distribution<double> dbl_dist(-1000.0, 1000.0);

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        PersistedEvent ev;
        ev.event_id = i;
        ev.thread_id = i % 250;
        ev.per_thread_event_id = i % 4000;
        ev.flags = (i % 17 == 0) ? (1ULL << 1) : 0;   // occasional KeeperRecord
        ev.category = "DEMO";
        ev.payload = "double_buffer_demo_payload_" + std::to_string(i);
        ev.timestamp_us = i * 1000;

        ev.int_metrics.resize(INT_COUNT);
        ev.dbl_metrics.resize(DBL_COUNT);
        for (auto& v : ev.int_metrics) v = int_dist(rng);
        for (auto& v : ev.dbl_metrics) v = dbl_dist(rng);

        writer.submit_event(std::move(ev));
    }

    writer.flush();
    writer.finalize();

    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double eps = static_cast<double>(NUM_EVENTS) * 1'000'000.0 / static_cast<double>(us);
    std::cout << "Submitted " << NUM_EVENTS << " events in " << us << " µs (including background drain)\n";
    std::cout << "Effective rate: " << static_cast<long long>(eps) << " events/sec\n";
}

int main() {
    std::cout << "=== ts_store Double-Buffered Persistence Demo ===\n";
    std::cout << "Demonstrates plug-and-play sinks (jText vs Binary)\n";

    // --- Using jText backend ---
    run_with_sink("JTextEventSink",
        std::make_unique<JTextEventSink>("DoubleBuf_JText", 9, 6));

    // --- Using Binary backend ---
    run_with_sink("BinaryEventSink",
        std::make_unique<BinaryEventSink>("DoubleBuf_Binary", 9, 6));

    std::cout << "\nDemo complete. Inspect the output files (DoubleBuf_*).\n";
    std::cout << "The same DoubleBufferedWriter + submit_event() API works for both.\n";

    return 0;
}
