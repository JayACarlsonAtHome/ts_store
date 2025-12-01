// ts_store_stress.cpp
// Full stress test with per-thread success/null tracking + results store

// =============================================================================
// DESIGN NOTE / KNOWN LIMITATION – Memory Guard
// =============================================================================
//
// The current memory_guard runs once per unique ts_store instantiation type
// (i.e. per distinct set of template parameters).
// It correctly estimates and checks memory for that single type only.
//
// When multiple ts_store objects with different template parameters exist in the
// same process (e.g. one with 100 B payload and another with 200 B), each gets
// its own independent guard. The guards do NOT sum their requirements, so the
// total memory usage of the process is not validated against available RAM.
//
// Impact: On systems with tight memory, it is theoretically possible to exceed
// available memory without the guard triggering — although the 150 MiB safety
// margin makes this unlikely in practice.
//
// Recommended future fix:
//   • Use a single ts_store specialization (same payload size) everywhere, or
//   • Implement a global static memory budget tracker shared across all instances.
//
// This is the only known correctness limitation as of 2025-11-30
// =============================================================================

#include "../ts_store_headers/ts_store.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace jac::ts_store::inline_v001;

int main()
{
    constexpr uint32_t THREADS = 250;
    constexpr uint32_t EVENTS_PER_THREAD = 100;
    constexpr uint64_t EXPECTED_COUNT = uint64_t(THREADS) * EVENTS_PER_THREAD;

    constexpr size_t MAIN_PAYLOAD    = 100;
    constexpr size_t RESULT_PAYLOAD  = 128;
    constexpr size_t TYPE_SIZE       = 16;
    constexpr size_t CATEGORY_SIZE   = 32;
    constexpr bool   USE_TS          = true;

    using MainStore   = ts_store<MAIN_PAYLOAD,   TYPE_SIZE, CATEGORY_SIZE, USE_TS>;
    using ResultStore = ts_store<RESULT_PAYLOAD, TYPE_SIZE, CATEGORY_SIZE, USE_TS>;

    MainStore   safepay(THREADS, EVENTS_PER_THREAD);
    ResultStore results(THREADS, 1);

    std::vector<std::thread> threads;
    std::atomic<int> total_successes{0};
    std::atomic<int> total_nulls{0};

    auto worker = [&](int tid)
    {
        int local_successes = 0;
        int local_nulls = 0;

        for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
            std::string payload_str = "payload-" + std::to_string(tid) + "-" + std::to_string(i);
            std::string_view payload(payload_str);

            auto [claim_ok, id] = safepay.claim(tid, payload, "STRESS", "MAIN");
            if (!claim_ok) {
                std::cerr << "Claim fail tid " << tid << " i " << i << "\n";
                continue;
            }

            std::this_thread::yield();
            std::atomic_thread_fence(std::memory_order_acquire);

            auto [val_ok, val_sv] = safepay.select(id);
            if (val_ok && val_sv == payload) {
                ++local_successes;
            } else if (!val_ok) {
                ++local_nulls;
            }
        }

        total_successes += local_successes;
        total_nulls += local_nulls;

        // Store result
        std::string result_str = "tid: " + std::to_string(tid) +
                                 " succ: " + std::to_string(local_successes) +
                                 " nulls: " + std::to_string(local_nulls);
        std::string_view result_view(result_str);

        auto [ok, out_id] = results.claim(tid, result_view, "RESULT", "STATS");
        if (!ok) {
            std::cerr << "Results claim failed for tid " << tid << "\n";
        }
    };

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();

    // Results
    auto all_result_ids = results.get_all_ids();
    std::sort(all_result_ids.begin(), all_result_ids.end());

    std::cout << std::left << std::setw(22) << "Safepay Size: (Expected: " << EXPECTED_COUNT << ")"
              << std::right << std::setw(8) << safepay.size() << "\n";
    safepay.show_duration("Safepay");
    results.show_duration("Results");

    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Total Successes:"
              << std::right << std::setw(7) << total_successes << " / " << EXPECTED_COUNT
              << (total_successes == EXPECTED_COUNT ? " PASS\n" : " FAIL\n");
    std::cout << std::left << std::setw(20) << "Nulls (races):"
              << std::right << std::setw(19) << total_nulls << "\n\n";

    const std::string line(80, '-');
    std::cout << line << "\n";
    std::cout << " Final per-thread results\n";

    safepay.press_any_key();


    std::cout << line << "\n";
    std::cout << std::left
              << std::setw(10) << "ID"
              << std::setw(12) << "TID"
              << std::setw(12) << "SUCCESS"
              << std::setw(12) << "NULLS"
              << (USE_TS ? std::setw(16) : std::setw(0)) << "TIMESTAMP (µs)\n";
    std::cout << std::string(80, '-') << "\n";

    for (auto rid : all_result_ids) {
        auto [ok, sv] = results.select(rid);
        if (!ok) {
            std::cout << std::setw(10) << rid << " <missing>\n";
            continue;
        }

        // Parse: "tid: X succ: Y nulls: Z"
        int tid = 0, succ = 0, nulls = 0;
        size_t p = sv.find("tid: ");     if (p != sv.npos) p += 5;
        size_t q = sv.find(" succ: ");   if (q != sv.npos) { tid = std::stoi(std::string(sv.substr(p, q-p))); q += 7; }
        size_t r = sv.find(" nulls: ");  if (r != sv.npos) { succ = std::stoi(std::string(sv.substr(q, r-q))); nulls = std::stoi(std::string(sv.substr(r+8))); }

        uint64_t ts = 0;
        if (USE_TS) {
            auto [tok, t] = results.get_timestamp_us(rid);
            if (tok) ts = t;
        }

        std::cout << std::left
            << std::setw(10) << rid
            << std::setw(12) << tid
            << std::setw(12) << succ
            << std::setw(12) << nulls;
        if (USE_TS) std::cout << std::setw(16) << ts;
        std::cout << "\n";
    }

    std::cout << line << "\n\n";


    safepay.print();
    safepay.show_duration();


    return 0;
}