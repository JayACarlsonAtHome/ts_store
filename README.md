### Massive changes, and minimal testing...
You can use this. I wouldn't use this in production.
I have limited time, a day here, a day there.
So I usually get as much as I can do in one night,
and it usually works out to get everything compiling, and runnning
in the morning, when I don't have that extra to do more testing.
That said, most all the tests were failing, and now they are not,
so that is a good thing.

### Why this exists
You needed an event buffer that:
- never drops or corrupts data
- never allocates on the hot path after `reserve()`
- gives perfect global order via timestamps
- survives million-operation stress tests
- modified from C++23 to C++17, 
----and back to C++20 for concepts, and other stuff.

### Features
- Memory check before run to ensure memory is available
- Fixed-size payload (adjustable, default 80 bytes -- template parameter)
- Microsecond timestamps relative to first claim (human-readable, zero cost)
- `claim()` returns monotonic 64-bit ID -- this is going to be renamed to save_event()
- Reader-writer lock → unlimited concurrent readers
- GTL `parallel_flat_hash_map` backend (the fastest thing that exists for this pattern)
- Five different tests from hello world at test_001 
---- to 1 Million records, repeated 50 times in quick succession.
- Pretty formatted output, sortable by ID, Time, or Thread

### If you get compiling failures
You may have to comment out this line in the CMakeLists.txt file</br>
   add_compile_options(--param destructive-interference-size=64)</br>
Then second compile, the opposite way.</br>
And depending on your system you might do it the other way around.</br>

### Test Compiling
```cpp
 // ts_store_001/test_001.cpp — 

#include "../ts_store_headers/ts_store.hpp"
#include <iostream>

using namespace jac::ts_store::inline_v001;
using LogStore = ts_store<
    fixed_string<96>,
    fixed_string<12>,
    fixed_string<24>,
    96, 12, 24,
    true        // UseTimestamps
>;

int main() {
    constexpr uint32_t threads = 5;
    constexpr uint32_t events  = 3;
    std::cout << "=== ts_store — TDD DEMO ===\n\n";
    std::cout << "PRODUCTION SIMULATION (full test verification)\n";
    
    LogStore prod(threads, events);
    prod.test_run();  //Test Run sets Debug == True
    if (!prod.verify_integrity()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — structural corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    if (!prod.verify_test_payloads()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — test payload corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    std::cout << "PRODUCTION SIMULATION PASSED — 100% clean\n";
    prod.press_any_key();
    prod.print();
    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    return 0;
}

=== ts_store — TDD DEMO ===

PRODUCTION SIMULATION (full test verification)
Memory guard: Threads: 5, Events: 3,  (0k) 
     Payload:       96 Bytes, 
     Type:          12 Bytes, 
     Category:      24 Bytes, 
  Total Bytes:     150 MiB (Padded for safety) 

   ***  RAM check: PASSED  ***

     [VERIFY] ALL 15 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 15 TEST PAYLOADS VALID
PRODUCTION SIMULATION PASSED — 100% clean

Press ENTER to continue...


ts_store < 
   Threads    = 5
   Events     = 3
   ValueT     = Trivially Copyable String
   Time Stamp = On>
========================================================================================================================================================================================================
ID          TIME (µs)     TYPE                CATEGORY                    THREAD    PAYLOAD
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
0           55            INFO                NET                         0         Test-Event: T=0 E=0
1           57            WARN                NET                         0         Test-Event: T=0 E=1
2           58            ERROR               NET                         0         Test-Event: T=0 E=2
3           63            INFO                DB                          1         Test-Event: T=1 E=0
4           64            WARN                DB                          1         Test-Event: T=1 E=1
5           65            ERROR               DB                          1         Test-Event: T=1 E=2
6           82            INFO                UI                          2         Test-Event: T=2 E=0
7           83            WARN                UI                          2         Test-Event: T=2 E=1
8           83            ERROR               UI                          2         Test-Event: T=2 E=2
9           111           INFO                SYS                         3         Test-Event: T=3 E=0
10          113           WARN                SYS                         3         Test-Event: T=3 E=1
11          113           ERROR               SYS                         3         Test-Event: T=3 E=2
12          128           INFO                GFX                         4         Test-Event: T=4 E=0
13          129           WARN                GFX                         4         Test-Event: T=4 E=1
14          130           ERROR               GFX                         4         Test-Event: T=4 E=2
========================================================================================================================================================================================================
Total entries: 15 (expected: 15)


=== ALL TESTS COMPLETED SUCCESSFULLY ===

Process finished with exit code 0
```

### Closer to real use
```cpp
// final_massive_test.cpp — 250 threads × 4000 events = 1,000,000 entries
// 50-run benchmark with min/max/avg — FINAL, UNBREAKABLE, PERFECT

#include "../ts_store_headers/ts_store.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <format>
#include <algorithm>

using namespace jac::ts_store::inline_v001;
using namespace std::chrono;

constexpr uint32_t THREADS           = 250;
constexpr uint32_t EVENTS_PER_THREAD = 4000;
constexpr uint64_t TOTAL             = uint64_t(THREADS) * EVENTS_PER_THREAD;
constexpr int      RUNS              = 50;

using MassiveStore = ts_store<
    fixed_string<100>,
    fixed_string<16>,
    fixed_string<32>,
    100, 16, 32,
    true
>;

int run_single_test(MassiveStore& store)
{
    store.clear();

    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (uint32_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (uint32_t i = 0; i < EVENTS_PER_THREAD; ++i) {
                auto payload = store.make_test_payload(t, i);
                auto [ok, id] = store.claim(t, payload, "MASSIVE", "FINAL");
                if (!ok) {
                    std::cerr << "CLAIM FAILED — thread " << t << " event " << i << "\n";
                    std::abort();
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto end = high_resolution_clock::now();
    auto write_us = duration_cast<microseconds>(end - start).count();

    if (!store.verify_integrity()) {
        std::cerr << "STRUCTURAL VERIFICATION FAILED\n";
        store.diagnose_failures();
        return -1;
    }

#ifdef TS_STORE_ENABLE_TEST_CHECKS
    if (!store.verify_test_payloads()) {
        std::cerr << "TEST PAYLOAD VERIFICATION FAILED\n";
        store.diagnose_failures();
        return -1;
    }
#endif

    return static_cast<int>(write_us);
}

int main()
{
    MassiveStore store(THREADS, EVENTS_PER_THREAD);

    long long total_write_us = 0;
    int64_t   durations[RUNS] = {};  // ← FIXED NAME
    int       failed_runs = 0;

    std::cout << "=== FINAL MASSIVE TEST — 1,000,000 entries × " << RUNS << " runs ===\n";
    std::cout << "Using store.clear() — fastest, most realistic reuse\n\n";

    for (int run = 0; run < RUNS; ++run) {
        std::cout << "Run " << std::setw(2) << (run + 1) << " / " << RUNS << "\n";

        int result = run_single_test(store);

        if (result < 0) {
            std::cout << "FAILED\n";
            ++failed_runs;
        } else {
            total_write_us += result;
            durations[run] = result;  // ← FIXED

            double ops_per_sec = 1'000'000.0 * 1'000'000.0 / result;

            std::cout << "PASS — "
                      << std::setw(8) << result << " µs → "
                      << std::fixed << std::setprecision(0)
                      << std::setw(9) << static_cast<uint64_t>(ops_per_sec + 0.5)
                      << " ops/sec\n\n";
        }
    }

    if (failed_runs > 0) {
        std::cout << "\n" << failed_runs << " runs failed — aborting summary.\n";
        return 1;
    }

    auto [min_it, max_it] = std::minmax_element(std::begin(durations), std::end(durations));
    double avg_us = static_cast<double>(total_write_us) / RUNS;
    double min_ops_sec = 1'000'000.0 * 1'000'000.0 / *max_it;
    double max_ops_sec = 1'000'000.0 * 1'000'000.0 / *min_it;
    double avg_ops_sec = 1'000'000.0 * 1'000'000.0 / avg_us;

    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "               FINAL RESULT — " << RUNS << "-RUN STATISTICS            \n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Fastest run        : " << std::setw(9) << *min_it << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(max_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  Slowest run        : " << std::setw(9) << *max_it << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(min_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  Average            : " << std::setw(9) << avg_us     << " µs  → "
              << std::setw(10) << static_cast<uint64_t>(avg_ops_sec + 0.5) << " ops/sec\n";
    std::cout << "  (1,000,000 events per run, 100% verified, zero corruption)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}
Real measured result on Bare Metal *2 (below) --> (Power setting: Performance, no tuning):
Results below:
Memory guard: Threads: 250, Events: 4000,  (1000k) 
     Payload:      100 Bytes, 
     Type:          16 Bytes, 
     Category:      32 Bytes, 
  Total Bytes:     344 MiB (Padded for safety) 

   ***  RAM check: PASSED  ***

=== FINAL MASSIVE TEST — 1,000,000 entries × 50 runs ===
Using store.clear() — fastest, most realistic reuse

Run  1 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   660228 µs →   1514628 ops/sec

Run  2 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   504940 µs →   1980433 ops/sec

Run  3 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   528099 µs →   1893584 ops/sec

Run  4 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   528170 µs →   1893330 ops/sec

Run  5 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   517996 µs →   1930517 ops/sec

Run  6 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   478102 µs →   2091604 ops/sec

Run  7 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   499773 µs →   2000908 ops/sec

Run  8 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   466707 µs →   2142672 ops/sec

Run  9 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   455665 µs →   2194595 ops/sec

Run 10 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   856636 µs →   1167357 ops/sec

Run 11 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   523411 µs →   1910544 ops/sec

Run 12 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   605464 µs →   1651626 ops/sec

Run 13 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   577995 µs →   1730119 ops/sec

Run 14 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   535171 µs →   1868562 ops/sec

Run 15 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   644517 µs →   1551549 ops/sec

Run 16 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   498044 µs →   2007855 ops/sec

Run 17 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   497360 µs →   2010616 ops/sec

Run 18 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   507417 µs →   1970766 ops/sec

Run 19 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   461560 µs →   2166566 ops/sec

Run 20 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   549329 µs →   1820403 ops/sec

Run 21 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   529026 µs →   1890266 ops/sec

Run 22 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   531111 µs →   1882846 ops/sec

Run 23 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   457969 µs →   2183554 ops/sec

Run 24 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   488859 µs →   2045580 ops/sec

Run 25 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   463259 µs →   2158620 ops/sec

Run 26 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   500910 µs →   1996367 ops/sec

Run 27 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   504959 µs →   1980359 ops/sec

Run 28 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   474252 µs →   2108584 ops/sec

Run 29 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   468384 µs →   2135000 ops/sec

Run 30 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   471186 µs →   2122304 ops/sec

Run 31 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   876967 µs →   1140294 ops/sec

Run 32 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   518849 µs →   1927343 ops/sec

Run 33 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   499625 µs →   2001501 ops/sec

Run 34 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   530025 µs →   1886703 ops/sec

Run 35 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   849458 µs →   1177221 ops/sec

Run 36 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   737066 µs →   1356731 ops/sec

Run 37 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   460460 µs →   2171741 ops/sec

Run 38 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   904526 µs →   1105551 ops/sec

Run 39 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   565521 µs →   1768281 ops/sec

Run 40 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   505822 µs →   1976980 ops/sec

Run 41 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   464270 µs →   2153919 ops/sec

Run 42 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   543736 µs →   1839128 ops/sec

Run 43 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   469726 µs →   2128901 ops/sec

Run 44 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   459334 µs →   2177065 ops/sec

Run 45 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   513704 µs →   1946646 ops/sec

Run 46 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   476041 µs →   2100659 ops/sec

Run 47 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   482204 µs →   2073811 ops/sec

Run 48 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   509673 µs →   1962042 ops/sec

Run 49 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   546131 µs →   1831063 ops/sec

Run 50 / 50
     [VERIFY] ALL 1000000 ENTRIES STRUCTURALLY PERFECT
[TEST-VERIFY] ALL 1000000 TEST PAYLOADS VALID
PASS —   752098 µs →   1329614 ops/sec

═══════════════════════════════════════════════════════════════
               FINAL RESULT — 50-RUN STATISTICS            
═══════════════════════════════════════════════════════════════
  Fastest run        :    455665 µs  →    2194595 ops/sec
  Slowest run        :    904526 µs  →    1105551 ops/sec
  Average            :    549035 µs  →    1821379 ops/sec
  (1,000,000 events per run, 100% verified, zero corruption)
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
| Bare metal RHEL 9.7       *2        | g++ 15.1.1     |  20 cores      | 1.1 - 2.2 million |

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


