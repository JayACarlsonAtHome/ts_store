# ts_store

A lightweight, thread-safe key-value store for C++ with auto-incrementing IDs, bundled thread IDs, and optional timestamps. Built for ease of use in concurrent apps like task queues or event logging, no data races, just claim and select. Scales to 100k ops at 1k threads (~6μs/op) with zero losses.

## Why ts_store?
Concurrent storage is a pain: races, lost inserts, hard debugging. ts_store hides it all—serialize writes safely, reads, auto-track IDs for sorting (by ID/tid/time/value). Timestamps make tricky problems (e.g., "why did thread 42 lag?") a breeze to trace.

## Features
Auto-Sequential IDs: Atomic uint64_t generation—no collisions, no manual keys.
Bundled Payloads: Each entry packs thread_id + fixed-char value + optional timestamp (default 80-byte char buffer; customizable).
Runtime TS Toggle: Enable/disable timestamps at init (no perf penalty when off).
Pair<bool, T> Returns: Simple success flag + result/error (fast, zero-alloc on success; int error codes like 1=NotFound, 2=TooLong).
Pre-Reserve: reserve(n) avoids rehashing in high-volume workloads.
Auto-ID Tracking with Sort Modes: Capture claimed IDs; sort by insertion, TID (thread Id), timestamp, or value 

## Installation
1. Clone: `git clone --recursive https://github.com/JayACarlsonAtHome/ts_store.git` (GTL submodule).
2. CMake 4.0 / CLion / Linux Mint (C++17, x64 Debug/Release).
3. Build->Clean Project
4. Build->Build Project
5. ========== Build: 4 succeeded, 0 failed, 0 up-to-date, 0 skipped ==========<br>
   ========== Build completed at 8:12 PM and took 03.805 seconds =========

## Usage
```cpp

// tid = Thread ID
// TS = TimeStamp


#include <iostream>  // For printout
#include <chrono>    // For steady_clock
#include "../ts_store.hpp"

int main() {  // Standalone compile
    ts_store<80> store(true);  // TS on (BufferSize=80 default)
    store.reserve(1000);  // Pre-alloc

    // Claim (insert with auto-ID) - twice for duration demo
    auto [ok1, id1] = store.claim(42, std::string_view("payload"));  // tid 42, value "payload"
    if (!ok1) {
        std::cout << "Insert failed (err code: " << static_cast<int>(id1) << ")" << "\n";
    }
    auto [ok2, id2] = store.claim(43, std::string_view("payload2"));  // Second insert (new ID)
    if (!ok2) {
        std::cout << "Insert failed (err code: " << static_cast<int>(id2) << ")" << "\n";
    }

    auto now = std::chrono::steady_clock::now();  // After claims for age

    // Select (lookup) first
    auto [val_ok, val] = store.select(id1);  // string_view to "payload"
    if (val_ok && val == std::string_view("payload")) {
        std::cout << "Match found: " << val << std::endl;  // Print confirmation
    }

    // Timestamp (if enabled) - first
    auto [ts_ok, ts] = store.get_timestamp(id1);  // steady_clock::time_point
    if (ts_ok) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - ts).count();
        std::cout << "Timestamp age: " << duration << " us" << std::endl;
    }

    // Duration summary 
    store.show_duration("Store");
    return 0;
}
