
### Why this exists
You needed an event buffer that:
- never drops or corrupts data
- never allocates on the hot path after `reserve()`
- gives perfect global order via timestamps
- survives million-operation stress tests
- modified from C++23 to C++17 so real older codebases can use it easier

Most solutions fail at least one of those.  
This one doesn’t.

### Features
- Memory check before run to ensure memory is available
- Fixed-size payload (adjustable, default 80 bytes -- template parameter)
- Microsecond timestamps relative to first claim (human-readable, zero cost)
- `claim()` returns monotonic 64-bit ID
- Reader-writer lock → unlimited concurrent readers
- GTL `parallel_flat_hash_map` backend (the fastest thing that exists for this pattern)
- Five independent stress tests, including the infamous `final_massive_test`
- Pretty formatted output, sortable by ID, Time, or Thread

### Testing compiling
```cpp
#include "../ts_store.hpp" // Uses BufferSize=80 default, pair<bool,...> returns

int main() {
    constexpr int Threads           = 2;
    constexpr int WorkersPerThread  = 5;
    constexpr int BufferSize        = 100;
    constexpr bool UseTimeStamps    = true;

    ts_store<Threads, WorkersPerThread, BufferSize, UseTimeStamps> store;
    store.test_run();   // Forces UseTimeStamps=true and Debug=true
    store.print();
}

Results below:
Memory guard: 2-threads 5-ops test (0k entries) [timestamps]
   Required      :      150 MiB
   Available now :    48999 MiB
   RAM check: PASSED

Test complete: 10 / 10 successes, 0 visibility races (should be 0)

ts_store<2,5,100>
========================================================================/  /====================
        ID        TIME (µs)   TYPE       THREAD  PAYLOAD
------------------------------------------------------------------------/  /---------------------
         0        -            Debug           0  payload-0-0.........../  /.....................
         1        22           Debug           0  payload-0-1.........../  /.....................
         2        22           Debug           0  payload-0-2.........../  /.....................
         3        23           Debug           0  payload-0-3.........../  /.....................
         4        24           Debug           0  payload-0-4.........../  /.....................
         5        209          Debug           1  payload-1-0.........../  /.....................
         6        211          Debug           1  payload-1-1.........../  /.....................
         7        214          Debug           1  payload-1-2.........../  /.....................
         8        214          Debug           1  payload-1-3.........../  /.....................
         9        215          Debug           1  payload-1-4.........../  /.....................
========================================================================/  /=====================
Total entries: 10

Process finished with exit code 0
```

### Closer to real use
```cpp
//
// final_massive_test.cpp
//
#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>

int main() {
    constexpr int THREADS = 250;
    constexpr int WORKERS_PER_THREAD = 4000;
    constexpr int TOTAL = THREADS * WORKERS_PER_THREAD;
    constexpr size_t BUFFER_SIZE = 100;

    ts_store<THREADS, WORKERS_PER_THREAD, 80, true> store;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &store]() {
            for (int i = 0; i < WORKERS_PER_THREAD; ++i) {
                // Zero-allocation payload — the real win
                auto payload = FastPayload<BUFFER_SIZE>::make(t, i);
                auto [ok, id] = store.claim(t, i);  // calls FastPayload overload
                if (!ok || id >= 1ull << 50) {
                    std::cerr << "CLAIM FAILED AT " << t << "/" << i << "\n";
                    std::abort();
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    auto mid = std::chrono::high_resolution_clock::now();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    if (ids.size() != TOTAL) {
        std::cout << "LOST " << TOTAL - ids.size() << " ENTRIES\n";
        return 1;
    }

    for (uint64_t i = 0; i < TOTAL; ++i) {
        auto [ok, val] = store.select(i);
        if (!ok || val.find("payload-") != 0) {
            std::cout << "CORRUPTION AT ID " << i << "\n";
            return 1;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto write_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto read_us  = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();

    std::cout << "Massive Test Passed\n";
    std::cout << "1,000,000 writes in " << write_us << " µs → "
              << (1'000'000 * 1'000'000.0 / write_us) << " ops/sec\n";
    std::cout << "1,000,000 sequential reads in " << read_us << " µs\n";

    return 0;
}

Results below:
Memory guard: 250-threads 4000-ops test (1000k entries) [timestamps]
   Required      :      287 MiB
   Available now :    48434 MiB
   RAM check: PASSED

Massive Test Passed
1,000,000 writes in 666107 µs → 1.50126e+06 ops/sec
1,000,000 sequential reads in 110207 µs

Process finished with exit code 0
```

### When to use it
Lock contention diagnostics</br>
Real-time telemetry / tracing</br>
Game engine event logs</br>
Fuzzing replay buffers</br>
Any high-throughput logging where dropping events is unacceptable </br>

### When NOT to use it
Persistence -- (coming soon)</br>
Trillions of events (use a real database)</br>
Variable-length strings longer than the fixed payload</br>

### Performance (measured 2025-11-28)

| Environment                         | Compiler       | Cores/Threads  | Writes/sec        |
|-------------------------------------|----------------|----------------|-------------------|
| RHEL 10.1 Virtual Machine *1        | g++ 15.1.1     |   8 threads    | 0.6 - 0.7 million |
| Bare metal RHEL 9.7       *2        | g++ 15.1.1     | 20 threads     | 1.3 - 1.6 million |

</br>
*1</br>
┌── System Info: VM ─────────────────────────────────────────</br>
│ OS      : Red Hat Enterprise Linux 10.1 (Coughlan)</br>
│ Kernel  : 6.12.0-124.13.1.el10_1.x86_64</br>
│ CPU     : Intel(R) Core(TM) Ultra 7 265 @ 8 threads</br>
│ Memory  : 4.9Gi/31Gi</br>
│ Uptime  : 15 minutes</br>
└────────────────────────────────────────────────────────────</br>
</br>
*2</br>
┌── System Info: Bare Metal──────────────────────────────────</br>
│ OS      : Red Hat Enterprise Linux 9.7 (Plow)</br>
│ Kernel  : 5.14.0-611.9.1.el9_7.x86_64</br>
│ CPU     : Intel(R) Core(TM) Ultra 7 265 @ 20 threads</br>
│ Memory  : 5.7Gi/61Gi</br>
│ Uptime  : 29 minutes</br>
└─────────────────────────────────────────────────────────</br>


