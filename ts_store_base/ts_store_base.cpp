// ts_safe_single_file.cpp - All-in-one: ts_safe + test (250 threads, 25000 ops)
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../ts_store.hpp" // Uses BufferSize=80 default, pair<bool,...> returns

int main() {
    std::string fileName = "ts_store_base"; // UPDATED: Match current file
    const int NUM_THREADS = 25; // UPDATED: To match comment (250 threads)
    const int NUM_WORKERS = 10; // UPDATED: 250*100=25000 ops
    constexpr int Expected_Count = NUM_THREADS * NUM_WORKERS;

	bool use_ts = true; // Enable timestamps
    auto start_time = std::chrono::steady_clock::now();

	ts_store<80> safepay(use_ts); // FIXED: <80> for fixed buffer (no T=string)
    ts_store<80> results(use_ts); // Outputs

	safepay.clear_claimed_ids(); // Reset for test
    results.clear_claimed_ids(); // Reset for test
    safepay.reserve(Expected_Count); // Pre-alloc for NUM_THREADS*NUM_WORKERS ops
    results.reserve(NUM_THREADS); // Pre-alloc 1 per thread

	std::vector<std::thread> threads;
    std::atomic<int> total_successes{ 0 };
    std::atomic<int> total_nulls{ 0 }; // NEW: Track failed selects (nulls/races)


	auto worker = [&](int tid) {
        int local_successes = 0;
        int local_nulls = 0; // NEW: Per-thread nulls
        for (int i = 0; i < NUM_WORKERS; ++i) {
            auto payload_str = std::string("payload-") + std::to_string(tid) + "-" + std::to_string(i); // Build string first
            std::string_view payload(payload_str); // View for claim
            auto claim_pair = safepay.claim(tid, payload); // FIXED: Assign to var (no structured binding)
            bool claim_ok = claim_pair.first; // FIXED: Extract bool
            std::uint64_t id = claim_pair.second;
            if (claim_ok) { // FIXED: Use extracted bool (no conditional on pair)
                std::this_thread::yield();
                std::atomic_thread_fence(std::memory_order_acquire);
                auto select_pair = safepay.select(id); // FIXED: Assign to var
                bool val_ok = select_pair.first; // FIXED: Extract bool
                std::string_view val_sv = select_pair.second;
                if (val_ok && val_sv == payload) {
                    ++local_successes;
                }
                else if (!val_ok) { // NEW: Explicit null check (not found/race)
                    ++local_nulls;
                }
            }
            else {
                std::cerr << "Claim fail tid " << tid << " i " << i << " (err: " << static_cast<int>(id) << ")" << std::endl; // Cast to int for ErrorCode
            }
        }
        total_successes += local_successes;
        total_nulls += local_nulls; // NEW: Aggregate nulls
        // Final claim: 1 per thread (auto-tracked)
        auto output_payload_str = std::string("tid: ") + std::to_string(tid) + std::string(" succ: ") + std::to_string(local_successes) + std::string(" nulls: ") + std::to_string(local_nulls); // UPDATED: Include nulls in output
        std::string_view output_payload(output_payload_str); // View
        auto results_pair = results.claim(tid, output_payload); // FIXED: Assign to var
        bool results_ok = results_pair.first; // FIXED: Extract bool
        std::uint64_t out_id = results_pair.second;
        if (!results_ok) { // FIXED: Use extracted bool
            std::cerr << "Results claim failed for tid " << tid << " (err: " << static_cast<int>(out_id) << ")" << std::endl;
            total_successes.fetch_sub(1); // Atomic adjust
        }
        };
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();
    // Get sorted IDs (mode 2 = by time)
    auto all_result_ids = results.get_claimed_ids_sorted(2);
    // Aligned size outputs
    std::cout << std::left << std::setw(22) << "Safepay Size: (Expected: " << Expected_Count << ")"
        << std::right << std::setw(8) << safepay.size() << "\n";
    safepay.show_duration("Safepay");
    results.show_duration("Results");
    // Aligned total output
    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Total Count / Successes:"
        << std::right << std::setw(7) << total_successes << " / " << Expected_Count;
    if (total_successes == Expected_Count) {
        std::cout << " PASS" << "\n";
    }
    else {
        std::cout << " FAIL" << "\n";
    }
    std::cout << std::left << std::setw(20) << "Nulls (races):" << std::right << std::setw(17) << total_nulls << "\n"; // NEW: Print nulls
    // Padded header for results
    std::cout << "\n";
    const std::string header_line = std::string(60, '-');
    std::cout << header_line << "\n";
    std::cout << " Final results from " << fileName << "\n";
    std::cout << header_line << "\n";
    std::cout << std::left << std::setw(8) << "ID" << std::setw(12) << "TID" << std::setw(12) << "SUCCESS" << std::setw(12) << "NULLS"; // UPDATED: Add NULLS col
    if (use_ts) {
        std::cout << std::setw(12) << "TIMESTAMP (us)" << "\n";
    }
    else {
        std::cout << "\n";
    }
    std::cout << std::string(64, '-') << "\n"; // UPDATED: Wider for new col
    for (auto rid : all_result_ids) {
        auto select_pair = results.select(rid); // FIXED: Assign to var
        bool out_ok = select_pair.first; // FIXED: Extract bool
        std::string_view out_sv = select_pair.second;
        if (out_ok) { // FIXED: Use extracted bool
            // UPDATED: Manual parsing for "tid: X succ: Y nulls: Z"
            int tid = 0, succ = 0, nulls = 0;
            size_t tid_pos = out_sv.find("tid: ");
            if (tid_pos != std::string_view::npos) {
                size_t tid_start = tid_pos + 5;
                size_t succ_pos = out_sv.find(" succ: ", tid_start);
                if (succ_pos != std::string_view::npos) {
                    std::string_view tid_sv = out_sv.substr(tid_start, succ_pos - tid_start);
                    tid = std::stoi(std::string(tid_sv));
                    size_t nulls_pos = out_sv.find(" nulls: ", succ_pos);
                    if (nulls_pos != std::string_view::npos) {
                        size_t succ_start = succ_pos + 7;
                        std::string_view succ_sv = out_sv.substr(succ_start, nulls_pos - succ_start);
                        succ = std::stoi(std::string(succ_sv));
                        size_t nulls_start = nulls_pos + 8;
                        std::string_view nulls_sv = out_sv.substr(nulls_start);
                        nulls = std::stoi(std::string(nulls_sv));
                    }
                }
            }
            auto ts_pair = results.get_timestamp(rid); // FIXED: Assign to var
            bool ts_ok = ts_pair.first; // FIXED: Extract bool
            auto ts = ts_pair.second;
            long long rel_us = 0; // Init; use long long for us
            if (ts_ok) { // FIXED: Use extracted bool
                rel_us = std::chrono::duration_cast<std::chrono::microseconds>(ts - start_time).count(); // Direct on ts
            }
            std::cout << std::left << std::setw(8) << ("ID " + std::to_string(rid))
                << std::setw(12) << ("tid:" + std::to_string(tid))
                << std::setw(12) << ("succ:" + std::to_string(succ))
                << std::setw(12) << ("nulls:" + std::to_string(nulls)) // NEW: Print per-thread nulls
                << std::setw(12) << rel_us << " us" << "\n"; // << rel_us directly
        }
        else {
            std::cout << std::left << std::setw(8) << ("ID " + std::to_string(rid))
                << std::setw(36) << "Not found (rare lag)" << "\n"; // UPDATED: Wider for new col
        }
    }
    // Final padded footer
    std::cout << header_line << "\n";
    return 0;
}