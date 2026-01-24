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

### Performance (measured 2026-01-13) -- Short Runs --
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |
|---------------------|------------|-------|------------|-----------------------------|
|                     |            |       |            | High    | Low     | Avg     |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 24.18M  | 12.44M  | 21.01M  | 
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 24.48M  | 13.16M  | 22.05M  | 
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 19.54M  | 16.05M  | 18.37M  |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 24.74M  | 17.66M  | 23.62M  |

*Writes: 250 threads × 4000 events over 50 runs (real data from test_005).*
*Average over 18 million operations per second with or without time stamps*
*Not Measured -- bulk validation of payload timing at end of test.
*Note 1: The simplification of code came at a 10 X speed improvement.*
*Note 2: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or ZIG Build of LLVM C++ on this run."


### Performance (measured 2026-01-24) -- Potentially thermally throttled...
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |
|---------------------|------------|-------|------------|-----------------------------|
|                     |            |       |            | High    | Low     | Avg     |
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 17.15M  | 10.38M  | 16.03M  | 
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 18.70M  | 12.47M  | 17.65M  | 
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 17.74M  | 13.36M  | 16.83M  |
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 19.49M  | 14.90M  | 18.59M  |

*Writes: 250 threads × 4000 events (1,000,000) over 1000 runs (real data from test_005).*
*Average over 16 million operations per second with or without timestamps*
*Not Measured -- bulk validation of payload timing at end of test.
*Note 1: More validation, lead to slightly slower code, but safer.
*Note 2: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or Zig Build of LLVM C++ on this run."


### Planned Features
- Double-buffered disk persistence
- Fast queries/filtering by Type, Category, or Payload
- Numeric values in payloads with math operations (sum, min, max, avg, etc.)
- Rollups/aggregates over Type, Category, or global
- Complex number support?

### Note
At this point I am generally happy with the code. There is still a little
more work to do in the color section, but that is minor to the project.
I added some more code, and it would seem it slowed down a little bit.
In actuality, it is about the same speed or faster. In short runs I was getting
as high as 27M TS, and 29M XS, but I am running a way more demanding test on 
it now, and the thermal buildup, is probably having a little bit of a hit, 
plus, the high and the average are pretty close together, meaning once in a while 
there is a cache hit, or something that is slowing it down occasionally.

### There are no guarantees on this code.
I have only run tests on 1 platform, other platforms, or chipsets may crash and burn,
So use it if you want, but make and run your own validation tests.

### Usage Examples
See the test files:  
- `test_001_TS.cpp` through `test_005_TS.cpp` → Timestamps enabled  
- `test_001_XS.cpp` through `test_005_XS.cpp` → Timestamps disabled
