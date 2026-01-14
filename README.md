# ts_store — Ultra-Fast, Thread-Safe Event Buffer

### Why this exists
- Simple, clean user interface — pick it up in minutes
- All the complicated concurrency/details hidden away
- Powerful backend optimized for extreme throughput and correctness

### Current Status — January 2026
**Extensively tested — zero corruption observed across all runs**  
All 10 stress tests pass 100% on g++ 15.1.1 (RHEL 9/10)  

**Not production-ready yet** — API is in flux and will change before final lock-down.

### Performance (measured 2026-01-12)
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |
|---------------------|------------|-------|------------|-----------------------------|
|                     |            |       |            | High    | Low     | Avg     |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | -----   | -----   | -----   | Untested at this time
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | -----   | -----   | -----   | Untested at this time
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 2.09M   | 1.11M   | 1.68M   |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 2.16M   | 0.89M   | 1.76M   |

### Performance (measured 2026-01-13) -- New Code --
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |
|---------------------|------------|-------|------------|-----------------------------|
|                     |            |       |            | High    | Low     | Avg     |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | -----   | -----   | -----   | Untested at this time
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | -----   | -----   | -----   | Untested at this time
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 19.54M  | 16.05M  | 18.37M  |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 24.74M  | 17.66M  | 23.62M  |


*Writes: 250 threads × 4000 events over 50 runs (real data from test_005).*
*Average over 18 million operations per second with or without time stamps*
*Not Measured -- bulk validation of payload timing at end of test.
*Note 1: The simplification of code came at a 10 X speed improvement.*
*Note 2: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or ZIG Build of LLVM C++ on this run."

### Planned Features
- Double-buffered disk persistence
- Fast queries/filtering by Type, Category, or Payload
- Numeric values in payloads with math operations (sum, min, max, avg, etc.)
- Rollups/aggregates over Type, Category, or global
- Complex number support?

### Note
Code is actively evolving and needs more polish.  
**NOT READY FOR PRIME TIME** — this is a work in progress.

### Usage Examples
See the test files:  
- `test_001_TS.cpp` through `test_005_TS.cpp` → Timestamps enabled  
- `test_001_XS.cpp` through `test_005_XS.cpp` → Timestamps disabled
