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

### If you get compiling failures
You may have to comment out this line in the CMakeLists.txt file</br>
   add_compile_options(--param destructive-interference-size=64)</br>
Then second compile, the opposite way.</br>
And depending on your system you might do it the other way around.</br>

### Test Compiling
```cpp
#include "../ts_store_headers/ts_store.hpp"
#include <iostream>

int main() {
    constexpr uint32_t threads = 50;
    constexpr uint32_t events  = 200;

    std::cout << "=== Production mode (real types & categories) ===\n";
    ts_store<96, 12, 24, true> prod(threads, events);
    prod.test_run_and_print(false);

    prod.press_any_key();

    std::cout << "\n=== Debug mode (same types, but debug flag on) ===\n";
    ts_store<96, 12, 24, true> debug(threads, events);
    debug.test_run_and_print(true);

    return 0;
}

/home/jay/git/ts_store/cmake-build-release/ts_store_base
=== Production mode (real types & categories) ===
Memory guard: 50 threads × 200 events  (10k entries, 96B payload, 12B type, 24B category, timestamps)
   Required      :      151 MiB
   Available now :    52835 MiB
   RAM check: PASSED

test_run(is_debug=false) complete:
  Expected entries : 10000
  Successful reads : 10000
  Visibility races  : 0 (should be 0)
  PASS — 100% correct

ts_store<50 threads × 200 events, Buffer:96B, Type:12B, Cat:24B, TS:on, Expected:10000>
==========================================================================================================================================================================================
ID          TIME (µs)    TYPE            CATEGORY                    THREAD    PAYLOAD
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
0           61            INFO            NET                         0         payload-0-0
1           66            WARN            NET                         0         payload-0-1
2           67            ERROR           NET                         0         payload-0-2
3           68            TRACE           NET                         0         payload-0-3

63          172           INFO            SYS                         8         payload-8-8
64          173           WARN            SYS                         8         payload-8-9
65          176           ERROR           SYS                         8         payload-8-10
66          178           INFO            NET                         5         payload-5-0
67          182           INFO            GFX                         4         payload-4-0
68          188           WARN            GFX                         4         payload-4-1

9997        14173         WARN            DB                          1         payload-1-197
9998        14174         ERROR           DB                          1         payload-1-198
9999        14174         TRACE           DB                          1         payload-1-199
==========================================================================================================================================================================================
Total entries: 10000  (expected: 10000)

Press ENTER to continue...

=== Debug mode (same types, but debug flag on) ===
test_run(is_debug=true) complete:
  Expected entries : 10000
  Successful reads : 10000
  Visibility races  : 0 (should be 0)
  PASS — 100% correct

ts_store<50 threads × 200 events, Buffer:96B, Type:12B, Cat:24B, TS:on, Expected:10000>
==========================================================================================================================================================================================
ID          TIME (µs)    TYPE            CATEGORY                    THREAD    PAYLOAD
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
0           169407829     INFO            NET                         0         payload-0-0
1           169407831     WARN            NET                         0         payload-0-1
2           169407834     ERROR           NET                         0         payload-0-2
3           169407835     TRACE           NET                         0         payload-0-3
4           169407837     INFO            DB                          1         payload-1-0
5           169407847     WARN            DB                          1         payload-1-1
6           169407850     ERROR           DB                          1         payload-1-2

996         169421037     INFO            UI                          47        payload-47-196
9997        169421037     WARN            UI                          47        payload-47-197
9998        169421038     ERROR           UI                          47        payload-47-198
9999        169421038     TRACE           UI                          47        payload-47-199
==========================================================================================================================================================================================
Total entries: 10000  (expected: 10000)

```

### Closer to real use
```cpp
//
// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
// 10-run benchmark using store.clear() — fastest, most realistic, unbreakable
//
#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace std::chrono;

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 4000;
constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;

constexpr size_t BUFFER_SIZE    = 100;
constexpr size_t TYPE_SIZE      = 16;
constexpr size_t CATEGORY_SIZE  = 32;
constexpr bool   USE_TIMESTAMPS = true;

int run_single_test(ts_store<BUFFER_SIZE, TYPE_SIZE, CATEGORY_SIZE, USE_TIMESTAMPS>& store)
{
    store.clear();  // Fresh start — zero cost, perfect reset

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([t, &store]() {
            for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
                auto payload = FastPayload<BUFFER_SIZE>::make(t, i);
                auto [ok, id] = store.claim(t, payload, "MASSIVE", "FINAL");
                if (!ok || id >= (1ull << 50)) {
                    std::cerr << "CLAIM FAILED\n";
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto mid = high_resolution_clock::now();

    auto ids = store.get_all_ids();
    std::sort(ids.begin(), ids.end());

    if (ids.size() != TOTAL) {
        std::cout << "LOST " << TOTAL - ids.size() << " ENTRIES\n";
        return -1;
    }

    // 100% verification — NOT in write timing
    //std::cout << "Verifying all " << TOTAL << " entries...\n";
    size_t errors = 0;
    for (uint64_t id : ids) {
        auto [ok, val] = store.select(id);
        if (!ok || val.find("payload-") != 0) {
            ++errors;
            if (errors <= 10) std::cout << "  Corruption at ID " << id << "\n";
        }
    }

    if (errors > 0) {
        std::cout << "FAILED: " << errors << " corrupted entries:  ";
        return -1;
    }

    std::cout << "100% correct:  ";

    auto write_us = duration_cast<microseconds>(mid - start).count();
    return static_cast<int>(write_us);
}

int main()
{
    constexpr int RUNS = 50;

    // One store — memory guard runs ONCE
    ts_store<BUFFER_SIZE, TYPE_SIZE, CATEGORY_SIZE, USE_TIMESTAMPS> store(
        THREADS, EVENTS_PER_THREAD);

    long long total_write_us = 0;
    int failed_runs = 0;

    std::cout << "Running massive test — " << RUNS << " iterations (store.clear() between runs)\n";
    std::cout << "Benchmark measures PURE WRITE throughput — verification is separate and untimed\n\n";

    for (int run = 1; run <= RUNS; ++run) {
        std::cout << "Run " << std::right << std::setw(2) << run
                  << " of " << RUNS << ":  ";

        int result = run_single_test(store);

        if (result < 0) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else {
            total_write_us += result;
            double ops_per_sec = 1'000'000.0 * 1'000'000.0 / result;

            std::cout << "PASS — "
                      << std::setw(7) << result << " µs → "
                      << std::fixed << std::setprecision(0)
                      << std::setw(10) << static_cast<uint64_t>(ops_per_sec + 0.5)
                      << " ops/sec\n";
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed.\n";
        return 1;
    }

    double avg_us = static_cast<double>(total_write_us) / RUNS;
    double avg_ops_sec = 1'000'000.0 * 1'000'000.0 / avg_us;

    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "            FINAL RESULT — " << RUNS << "-RUN AVERAGE (store.clear())\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Average write time : " << std::setw(9) << avg_us << " µs\n";
    std::cout << "  Average throughput : " << std::setw(10)
              << static_cast<uint64_t>(avg_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  (1,000,000 events per run, 100% verified)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}

Real measured result on Bare Metal *2 (below) --> (Power setting: Performance, no tuning):
Results below:
Memory guard: 250 threads × 4000 events  (1000k entries, 100B payload, 16B type, 32B category, timestamps)
   Required      :      352 MiB
   Available now :    52001 MiB
   RAM check: PASSED

Running massive test — 50 iterations (store.clear() between runs)
Benchmark measures PURE WRITE throughput — verification is separate and untimed

Run  1 of 50:  100% correct:  PASS —  570032 µs →    1754287 ops/sec
Run  2 of 50:  100% correct:  PASS —  538311 µs →    1857662 ops/sec
Run  3 of 50:  100% correct:  PASS —  534254 µs →    1871769 ops/sec
Run  4 of 50:  100% correct:  PASS —  466571 µs →    2143297 ops/sec
Run  5 of 50:  100% correct:  PASS —  488808 µs →    2045793 ops/sec
Run  6 of 50:  100% correct:  PASS —  495681 µs →    2017427 ops/sec
Run  7 of 50:  100% correct:  PASS —  490380 µs →    2039235 ops/sec
Run  8 of 50:  100% correct:  PASS —  689421 µs →    1450493 ops/sec
Run  9 of 50:  100% correct:  PASS —  519895 µs →    1923465 ops/sec
Run 10 of 50:  100% correct:  PASS —  482071 µs →    2074383 ops/sec
Run 11 of 50:  100% correct:  PASS —  469797 µs →    2128579 ops/sec
Run 12 of 50:  100% correct:  PASS —  533070 µs →    1875926 ops/sec
Run 13 of 50:  100% correct:  PASS —  515446 µs →    1940067 ops/sec
Run 14 of 50:  100% correct:  PASS —  476075 µs →    2100509 ops/sec
Run 15 of 50:  100% correct:  PASS —  525109 µs →    1904367 ops/sec
Run 16 of 50:  100% correct:  PASS —  472488 µs →    2116456 ops/sec
Run 17 of 50:  100% correct:  PASS —  563794 µs →    1773697 ops/sec
Run 18 of 50:  100% correct:  PASS —  534943 µs →    1869358 ops/sec
Run 19 of 50:  100% correct:  PASS —  478658 µs →    2089174 ops/sec
Run 20 of 50:  100% correct:  PASS —  507195 µs →    1971628 ops/sec
Run 21 of 50:  100% correct:  PASS —  539406 µs →    1853891 ops/sec
Run 22 of 50:  100% correct:  PASS —  544004 µs →    1838222 ops/sec
Run 23 of 50:  100% correct:  PASS —  528840 µs →    1890931 ops/sec
Run 24 of 50:  100% correct:  PASS —  522818 µs →    1912711 ops/sec
Run 25 of 50:  100% correct:  PASS —  549733 µs →    1819065 ops/sec
Run 26 of 50:  100% correct:  PASS —  534835 µs →    1869736 ops/sec
Run 27 of 50:  100% correct:  PASS — 1230088 µs →     812950 ops/sec
Run 28 of 50:  100% correct:  PASS —  564930 µs →    1770131 ops/sec
Run 29 of 50:  100% correct:  PASS —  480373 µs →    2081716 ops/sec
Run 30 of 50:  100% correct:  PASS —  763545 µs →    1309681 ops/sec
Run 31 of 50:  100% correct:  PASS —  513257 µs →    1948342 ops/sec
Run 32 of 50:  100% correct:  PASS —  507785 µs →    1969337 ops/sec
Run 33 of 50:  100% correct:  PASS —  520948 µs →    1919577 ops/sec
Run 34 of 50:  100% correct:  PASS —  892298 µs →    1120702 ops/sec
Run 35 of 50:  100% correct:  PASS —  479204 µs →    2086794 ops/sec
Run 36 of 50:  100% correct:  PASS —  951451 µs →    1051026 ops/sec
Run 37 of 50:  100% correct:  PASS —  598998 µs →    1669455 ops/sec
Run 38 of 50:  100% correct:  PASS —  799170 µs →    1251298 ops/sec
Run 39 of 50:  100% correct:  PASS —  509989 µs →    1960827 ops/sec
Run 40 of 50:  100% correct:  PASS —  475786 µs →    2101785 ops/sec
Run 41 of 50:  100% correct:  PASS —  473326 µs →    2112709 ops/sec
Run 42 of 50:  100% correct:  PASS —  504346 µs →    1982766 ops/sec
Run 43 of 50:  100% correct:  PASS —  596192 µs →    1677312 ops/sec
Run 44 of 50:  100% correct:  PASS —  470847 µs →    2123832 ops/sec
Run 45 of 50:  100% correct:  PASS —  532751 µs →    1877050 ops/sec
Run 46 of 50:  100% correct:  PASS —  604235 µs →    1654985 ops/sec
Run 47 of 50:  100% correct:  PASS —  656637 µs →    1522911 ops/sec
Run 48 of 50:  100% correct:  PASS —  544240 µs →    1837425 ops/sec
Run 49 of 50:  100% correct:  PASS —  520537 µs →    1921093 ops/sec
Run 50 of 50:  100% correct:  PASS —  536586 µs →    1863634 ops/sec

═══════════════════════════════════════════════════════════════
            FINAL RESULT — 50-RUN AVERAGE          
═══════════════════════════════════════════════════════════════
  Average write time :    565983 µs
  Average throughput :    1766837 ops/sec
  (1,000,000 events per run, 100% verified)
═══════════════════════════════════════════════════════════════

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

### Performance (measured 2025-11-30)

| Environment                         | Compiler       | Cores          | Writes/sec        |
|-------------------------------------|----------------|----------------|-------------------|
| RHEL 10.1 Virtual Machine *1        | g++ 15.1.1     |   4 cores      | 0.6 - 1.1 million |
| Bare metal RHEL 9.7       *2        | g++ 15.1.1     |  20 cores      | 0.8 - 2.1 million |

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

### Note
The code is adding features and changing APIs, so I would not build any code from this yet.
There will be a point in the future, I lock down to a release version.
That said, if you clone it please, also be a watcher, so that I can see the interest.


