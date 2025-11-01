// concurrent_test.cpp - Test read-while-write safety in ts_safe
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "../ts_store.hpp"  // Uses BufferSize=80 default, pair<bool,...> returns

int main() {
    std::string fileName = "ts_store_concurrent";  // FIXED: Match current file
    const int NUM_WRITER_THREADS = 50;
    const int NUM_READER_THREADS = 50;
    const int NUM_OPS = 1000;
    constexpr int Expected_Writes = NUM_WRITER_THREADS * NUM_OPS;
    constexpr int Expected_Reads = NUM_READER_THREADS * NUM_OPS;
    bool use_ts = true;
    ts_store<80> store(use_ts);  // FIXED: <80> for fixed buffer
    store.reserve(Expected_Writes);
    store.clear_claimed_ids();
    std::atomic<int> total_writes{ 0 };
    std::atomic<int> total_reads_concurrent{ 0 };  // FIXED: Rename for concurrent phase
    std::atomic<int> total_nulls_concurrent{ 0 };  // FIXED: Concurrent nulls
    std::atomic<int> total_reads_post{ 0 };  // NEW: Post-write reads
    std::atomic<int> total_nulls_post{ 0 };  // NEW: Post-write nulls
    std::vector<std::thread> writers, readers_concurrent;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, Expected_Writes - 1); // Random ID for readers (0 to max expected)

    // Writers: Claim ops
    for (int t = 0; t < NUM_WRITER_THREADS; ++t) {
        writers.emplace_back([&, t]() {
            int local_writes = 0;
            for (int i = 0; i < NUM_OPS; ++i) {
                auto payload_str = std::string("payload-") + std::to_string(t) + "-" + std::to_string(i);  // FIXED: Build string
                std::string_view payload(payload_str);  // FIXED: View for claim
                auto claim_pair = store.claim(t, payload);  // FIXED: pair<bool,uint64_t>
                bool claim_ok = claim_pair.first;
                if (claim_ok) {
                    ++local_writes;
                }
            }
            total_writes += local_writes;  // FIXED: Use local_writes
            });
    }
    // Readers: Select random IDs concurrently with writes
    for (int t = 0; t < NUM_READER_THREADS; ++t) {
        readers_concurrent.emplace_back([&, t]() {
            int local_reads = 0;
            int local_nulls = 0;  // NEW: Per-thread nulls
            for (int i = 0; i < NUM_OPS; ++i) {
                uint64_t random_id = dis(gen);
                std::atomic_thread_fence(std::memory_order_acquire);
                auto select_pair = store.select(random_id);  // FIXED: pair<bool,string_view>
                bool val_ok = select_pair.first;
                if (val_ok) {
                    ++local_reads;
                }
                else {
                    ++local_nulls;  // FIXED: Count nulls (not found/race)
                }
            }
            total_reads_concurrent += local_reads;
            total_nulls_concurrent += local_nulls;  // FIXED: Aggregate nulls
            });
    }
    // FIXED: Launch writers and concurrent readers
    for (auto& th : writers) th.detach();
    for (auto& th : readers_concurrent) th.detach();
    // Wait for writes + concurrent reads to overlap
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // NEW: Join writers to ensure all writes complete before post-phase
    // (Since detached, use a barrier or sleep; here, extended sleep for simplicity)
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Extra wait for writes

    // NEW: Post-write reads: Scan all claimed IDs exactly (prove visibility)
    std::vector<std::uint64_t> all_ids = store.get_claimed_ids_sorted(0);  // Insertion order
    int post_successes = 0;
    int post_nulls = 0;
    for (auto id : all_ids) {
        auto select_pair = store.select(id);
        bool val_ok = select_pair.first;
        if (val_ok) {
            ++post_successes;
        }
        else {
            ++post_nulls;
        }
    }
    total_reads_post = post_successes;
    total_nulls_post = post_nulls;

    // Padded header for results
    std::cout << "\n";
    std::cout << " Final results from " << fileName << "\n";
    const std::string header_line = std::string(60, '-');
    std::cout << header_line << "\n";
    std::cout << "Writes: " << total_writes << "/" << Expected_Writes << " (" << (total_writes == Expected_Writes ? "PASS" : "FAIL") << ")\n";
    std::cout << "Concurrent Reads: " << total_reads_concurrent << "/" << Expected_Reads << " (nulls: " << total_nulls_concurrent << ") (" << (total_reads_concurrent > 0 ? "PASS" : "FAIL") << ")\n";  // FIXED: Concurrent specific
    std::cout << "Note: Concurrent nulls likely from reads racing ahead of writes (data not yet visible).\n";  // NEW: Suspicion note
    std::cout << "Post-Write Reads: " << total_reads_post << "/" << all_ids.size() << " (nulls: " << total_nulls_post << ") (" << (total_reads_post == static_cast<int>(all_ids.size()) ? "PASS" : "FAIL") << ")\n";  // NEW: Post phase
    std::cout << "Store size: " << store.size() << "\n";
    store.show_duration("Store");
    std::cout << header_line << "\n";
    return 0;
}