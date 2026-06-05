// examples/in_memory_throughput.cpp
// Pure in-memory hot path throughput test (no persistence attached).
// Measures the save_event hot path using bounded_string (fixed inline char buffers)
// + direct assign_truncated (cp count + memcpy). Full feature set.
// Pre-created data with variety + strided access so we switch it up (no tiny
// repeat cheating, no per-event allocs in measured loop).

#include <beman/ts_store/ts_store_headers/ts_store.hpp>

#include <chrono>
#include <iostream>
#include <vector>

using namespace jac::ts_store::inline_v001;

int main() {
    constexpr size_t NUM_EVENTS = 1'000'000;
    constexpr size_t INT_COUNT = 9;
    constexpr size_t DBL_COUNT = 6;
    constexpr size_t THREADS = 8;  // low contention for "best case" hot path
    constexpr size_t EVENTS_PER = NUM_EVENTS / THREADS;

    std::cout << "=== Pure In-Memory Hot Path Throughput ===\n";
    std::cout << "Events: " << NUM_EVENTS << " | " << INT_COUNT << " ints + " << DBL_COUNT << " doubles\n";
    std::cout << "Threads: " << THREADS << " (low contention)\n\n";

    using Config = ts_store_config<true, 6, 20, 43, INT_COUNT, DBL_COUNT, false>;
    ts_store<Config> store(THREADS, EVENTS_PER);

    // NO persistence attached -- pure in-memory

    // Pre-create a decent amount of *distinct* data *outside* the timed loops.
    // We deliberately use larger pools (1024 payloads, 256 metric variants, 32 cats)
    // + strided mixing indices so the input data "switches it up" across the run.
    // This is not just re-writing the exact same 4 tiny items in a tight %4 loop
    // (which would be too cache-friendly and feel like cheating for a throughput
    // number). The strings and metrics are still pre-created — zero per-event
    // allocations, no to_string, no rng, no new strings inside the measurement.
    // Now using the store's bounded_string (no std::string at all in this benchmark path).

    constexpr size_t PAYLOAD_POOL = 1024;
    constexpr size_t CAT_POOL     = 32;
    constexpr size_t METRIC_POOL  = 256;

    std::array<Config::ValueT, PAYLOAD_POOL> pre_payloads;
    for (size_t i = 0; i < PAYLOAD_POOL; ++i) {
        // Distinct, short, well under the 43 codepoint max
        pre_payloads[i].assign_truncated("pl-" + std::to_string(i) + "-evt-data");
    }

    std::array<Config::CategoryT, CAT_POOL> pre_cats;
    for (size_t i = 0; i < CAT_POOL; ++i) {
        pre_cats[i].assign_truncated( std::string("CAT_") + char('A' + (i % 26)) );
    }

    std::array<std::array<int64_t, INT_COUNT>, METRIC_POOL> pre_ints{};
    std::array<std::array<double, DBL_COUNT>, METRIC_POOL> pre_dbls{};
    for (size_t s = 0; s < METRIC_POOL; ++s) {
        for (size_t k = 0; k < INT_COUNT; ++k) {
            pre_ints[s][k] = static_cast<int64_t>(s * 97 + k * 11 + (s >> 3));
        }
        for (size_t k = 0; k < DBL_COUNT; ++k) {
            pre_dbls[s][k] = static_cast<double>(s) * 0.019 + static_cast<double>(k) * 0.0071 + 0.5;
        }
    }

    std::array<uint64_t, 16> pre_flags{};
    for (size_t s = 0; s < 16; ++s) {
        uint64_t f = 0;
        f = set_user_flag(f, TsStoreFlags::UserFlag::KeeperRecord);
        f = set_severity(f, static_cast<TsStoreFlags::Severity>(s % 8));
        if (s & 1) f = set_user_flag(f, TsStoreFlags::UserFlag::HotCacheHint);
        if (s & 2) f = set_user_flag(f, TsStoreFlags::UserFlag::LogConsole);
        pre_flags[s] = f;
    }

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);
    size_t total = 0;

    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < EVENTS_PER; ++i) {
                // Strided indices so we actually switch through the pools instead of
                // tight tiny cycling. Still no allocations in the hot loop.
                size_t pidx = (i * 73 + t * 19) % PAYLOAD_POOL;
                size_t cidx = (i * 11 + t) % CAT_POOL;
                size_t midx = (i * 57 + t * 7) % METRIC_POOL;
                size_t fidx = (i + t * 3) % 16;

                const auto& pl   = pre_payloads[pidx];
                const auto& cat  = pre_cats[cidx];
                const auto& ints = pre_ints[midx];
                const auto& dbls = pre_dbls[midx];
                uint64_t flags   = pre_flags[fidx];

                // Pass the bounded values directly (no extra std::string).
                // save_event takes Config::ValueT&& / CategoryT&& and the bounded
                // does the direct assign_truncated into the row's storage.
                auto [ok, id] = store.save_event(
                    t, i, pl, flags, cat, true, ints, dbls
                );
                if (ok) {
                    // prevent over-optimization of the call
                    if ((id & 0xFFFFF) == 0) total += id;
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double eps = static_cast<double>(NUM_EVENTS) * 1'000'000.0 / static_cast<double>(us);

    std::cout << "Saved " << NUM_EVENTS << " events in " << us << " µs\n";
    std::cout << "Throughput: " << static_cast<long long>(eps + 0.5) << " events/sec\n\n";

    std::cout << "This is the pure in-memory hot path (no persistence submit cost).\n";
    std::cout << "Data is pre-created outside the loop (1024 distinct payloads, 256 metric\n";
    std::cout << "patterns, etc.) with strided access so we actually switch the values up.\n";
    std::cout << "No per-event allocations or construction — measures the store (save_event\n";
    std::cout << "+ bounded_string fixed buffers + direct assign_truncated writes).\n";
    std::cout << "Realistic workloads with async logging will be lower due to submit_event + data copying.\n";

    return 0;
}
