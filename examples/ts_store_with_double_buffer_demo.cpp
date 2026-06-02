// examples/ts_store_with_double_buffer_demo.cpp
//
// Integrated demo: ts_store + attach_persistence (double buffered, pluggable sinks)
//
// This shows the "wired up" experience:
// - Normal ts_store usage
// - One call to attach_persistence(...)
// - Background double-buffering to jText or Binary happens automatically on every save_event

#include <beman/ts_store/ts_store_headers/ts_store.hpp>

#include <beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp>
#include <beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp>
#include <beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace jac::ts_store::inline_v001;

int main() {
    std::cout << "=== ts_store with Double-Buffered Persistence (Integrated) ===\n\n";

    constexpr size_t THREADS = 8;
    constexpr size_t EVENTS_PER_THREAD = 5'000;   // 40k total events
    constexpr size_t INT_COUNT = 9;
    constexpr size_t DBL_COUNT = 6;

    // === Create normal ts_store (in-memory hot path) ===
    using Config = ts_store_config<true, 6, 32, 256, INT_COUNT, DBL_COUNT, false, false, false, false>;
    ts_store<Config> store(THREADS, EVENTS_PER_THREAD);

    // === Choose your sink (plug and play) ===
    // Uncomment one of the two blocks below to switch backend with zero change to the rest of the code.

    // --- jText version (human readable split files) ---
    auto jtext_sink = std::make_unique<JTextEventSink>("Integrated_JText", INT_COUNT, DBL_COUNT);
    auto jtext_writer = std::make_unique<DoubleBufferedWriter>(std::move(jtext_sink), 4'000);
    store.attach_persistence(std::move(jtext_writer));
    std::cout << "Attached: DoubleBufferedWriter + JTextEventSink (batch=4000)\n\n";

    /*
    // --- Binary version (fast production path) ---
    auto binary_sink = std::make_unique<BinaryEventSink>("Integrated_Binary", INT_COUNT, DBL_COUNT);
    auto binary_writer = std::make_unique<DoubleBufferedWriter>(std::move(binary_sink), 4'000);
    store.attach_persistence(std::move(binary_writer));
    std::cout << "Attached: DoubleBufferedWriter + BinaryEventSink (batch=4000)\n\n";
    */

    // === Generate realistic events ===
    std::mt19937 rng(123);
    std::uniform_int_distribution<int64_t> int_dist(-100000, 100000);
    std::uniform_real_distribution<double> dbl_dist(-1000.0, 1000.0);
    std::uniform_int_distribution<int> sev_dist(0, 7);

    auto start = std::chrono::steady_clock::now();

    size_t total = 0;

    for (size_t t = 0; t < THREADS; ++t) {
        for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {
            std::string payload = "event from thread " + std::to_string(t) + " seq " + std::to_string(i);

            std::array<int64_t, INT_COUNT> ints{};
            std::array<double, DBL_COUNT>  dbls{};

            for (auto& v : ints) v = int_dist(rng);
            for (auto& v : dbls) v = dbl_dist(rng);

            uint64_t flags = 0;
            flags = set_user_flag(flags, TsStoreFlags::UserFlag::KeeperRecord);   // persist this one
            flags = set_severity(flags, static_cast<TsStoreFlags::Severity>(sev_dist(rng)));

            auto [ok, id] = store.save_event(
                t, i,
                std::move(payload),
                flags,
                "DEMO",
                true,           // debug
                ints,
                dbls
            );

            if (ok) ++total;
        }
    }

    auto mid = std::chrono::steady_clock::now();

    // Give the background thread a moment to drain (in real use you would just let it run)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Explicit shutdown (flushes remaining + finalizes sinks)
    // In production you would typically let the ts_store dtor + writer dtor handle it,
    // or call flush on the writer if you kept a handle.
    store.clear(); // not strictly needed, just demo

    auto end = std::chrono::steady_clock::now();

    auto hot_path_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto total_us    = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Saved " << total << " events through ts_store hot path.\n";
    std::cout << "Hot path time: " << hot_path_us << " µs  (~" 
              << (static_cast<double>(total) * 1'000'000.0 / static_cast<double>(hot_path_us)) << " events/sec)\n";
    std::cout << "Total time (incl. background drain + sleep): " << total_us << " µs\n\n";

    std::cout << "Persistence happened in the background via the attached DoubleBufferedWriter.\n";
    std::cout << "Check the output files:\n";
    std::cout << "  - Integrated_JText.jtext + _Ints.jtext + _Floats.jtext   (if using jText sink)\n";
    std::cout << "  - Integrated_Binary.bin                                 (if using Binary sink)\n";

    return 0;
}
