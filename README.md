# ts_store — Ultra-Fast, Thread-Safe Event Buffer

### Why this exists
- Simple, clean user interface — pick it up in minutes
- All the complicated concurrency/details hidden away
- Powerful backend optimized for extreme throughput and correctness

### Current Status — January 2026
**Extensively tested — zero corruption observed across all runs**  
All 10 stress tests pass 100% on g++ 15.1.1 (RHEL 9/10)  

**Not production-ready yet** — API is in flux and will change before final lock-down.

### Performance (measured 2026-01-12)</br>
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |</br>
|---------------------|------------|-------|------------|-----------------------------|</br>
|                     |            |       |            | High    | Low     | Avg     |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | -----   | -----   | -----   | Untested at this time</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | -----   | -----   | -----   | Untested at this time</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 2.09M   | 1.11M   | 1.68M   |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 2.16M   | 0.89M   | 1.76M   |</br>
</br>

### Performance (measured 2026-01-13) -- Short Runs --</br>
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events)      |</br>
|---------------------|------------|-------|------------|-----------------------------|</br>
|                     |            |       |            | High    | Low     | Avg     |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 24.18M  | 12.44M  | 21.01M  |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 24.48M  | 13.16M  | 22.05M  |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 19.54M  | 16.05M  | 18.37M  |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 24.74M  | 17.66M  | 23.62M  |</br>
</br>
*Writes: 250 threads × 4000 events over 50 runs (real data from test_005).*</br>
*Average over 18 million operations per second with or without time stamps*</br>
*Not Measured -- bulk validation of payload timing at end of test.</br>
*Note 1: The simplification of code came at a 10 X speed improvement.*</br>
*Note 2: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or ZIG Build of LLVM C++ on this run."</br>

### Performance (measured 2026-01-26) -- Fastest Run in a while</br>
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events) </br>
|---------------------|------------|-------|------------|-------------|---------|---------|</br>
|                     |            |       |            | High        | Low     | Avg     |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 17.15M      | 10.38M  | 16.03M  |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 18.70M      | 12.47M  | 17.65M  |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 28.85M      | 18.68M  | 25.83M  |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 27.20M      | 20.43M  | 24.78M  |</br>
</br>
*Writes: 250 threads × 4000 events (1,000,000) over 1000 runs (real data from test_005).*</br>
*Average over 16 million operations per second with or without timestamps*</br>
*Not Measured -- bulk validation of payload timing at end of test.</br>
*Note 1: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or Zig Build of LLVM C++ on this run."</br>
*Note 2: While Average speed is almost always over 15M ops/sec aver, there can be a big difference on runs. </br>


### Performance (measured 2026-01-31) -- Back to Slow Mode...Will fix on next version...</br>
| Environment         | Compiler   | Cores | Timestamps | Writes/sec (1M events) </br>
|---------------------|------------|-------|------------|-------------|---------|---------|</br>
|                     |            |       |            | High        | Low     | Avg     |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | On         | 15.73M      | 13.79M  | 15.25M  |</br>
| RHEL 10.1 VM        | g++ 15.1.1 | 4     | Off        | 16.67M      | 13.72M  | 16.04M  |</br> 
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | On         | 18.10M      | 14.83M  | 17.17M  |</br>
| Bare metal RHEL 9.7 | g++ 15.1.1 | 20    | Off        | 18.31M      | 14.72M  | 17.61M  |</br>

*Writes: 250 threads × 4000 events (1,000,000) over 50 runs (real data from test_005).*</br>
*Average over 16 million operations per second with or without timestamps*</br>
*Not Measured -- bulk validation of payload timing at end of test.</br>
*Note 1: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or Zig Build of LLVM C++ on this run."</br>
*Note 2: While Average speed is almost always over 15M ops/sec aver, there can be a big difference on runs. </br>

### Planned Features -- Really it is coming first quarter 2026...
- Double-buffered disk persistence </br>
- Fast queries/filtering by Type, Category, or Payload </br>
- Numeric values in payloads with math operations (sum, min, max, avg, etc.) </br>
- Rollups/aggregates over Type, Category, or global </br>
- Complex number support? </br>

### Note
At this point I am generally happy with the code. There is still a little</br>
more work to do in the color section, but that is minor compared to the rest of the project.</br>
I added some more code, and it seems to have slowed down a little bit.</br>
I think the slowdown is due to using std::strings: when they are kept short you get </br>
SSO (small string optimization), but as they get longer, that no longer applies.</br>
So, since I am putting limits on the string sizes in the configuration, I may as well</br>
change the std::strings to fixed-size char arrays or std::arrays/std::vectors of char. </br>
Either way I will have to test and see what performs better. Also, in the future, I am always</br>
going to test on a warmed-up CPU so that if thermal throttling occurs, it is already factored in.</br>

### There are no guarantees on this code.
I have only run tests on 1 platform; other platforms or chipsets may crash and burn.</br>
So use it if you want, but make and run your own validation tests.</br>

### Usage Examples
See the test files:</br>
test_001... → very simple example</br>
test_001_TS.cpp through test_005_TS.cpp → Timestamps enabled</br>
test_001_XS.cpp through test_005_XS.cpp → Timestamps disabled</br>
                        test_005... → substantial numbers</br>

