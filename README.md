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
<img width="662" height="135" alt="image" src="https://github.com/user-attachments/assets/b64778fc-c8b5-4a18-99ca-55077a73b818" />
</br>
### Performance (measured 2026-01-26) -- Fastest Run in a while</br>
<img width="662" height="117" alt="image" src="https://github.com/user-attachments/assets/1a660830-217b-4007-a7a2-8cfa6f75f543" />
</br>
*Writes: 250 threads × 4000 events (1,000,000) over 1000 runs (real data from test_005).*</br>
*Average over 16 million operations per second with or without timestamps*</br>
*Not Measured -- bulk validation of payload timing at end of test.</br>
*Note 1: Test only run on GCC G++ version 15.1.1, no tests on LLVM\Clang or Zig Build of LLVM C++ on this run."</br>
*Note 2: While Average speed is almost always over 15M ops/sec aver, there can be a big difference on runs. </br>
</br>
### Performance (measured 2026-01-31) -- Back to Slow Mode...Will fix on next version...</br>
<img width="662" height="117" alt="image" src="https://github.com/user-attachments/assets/e2c3b2ee-f676-4458-a479-46655a3f6298" />
</br>
*Writes: 250 threads × 4000 events (1,000,000) over 50 runs (real data from test_005).*</br>
*Average over 15 million operations per second with or without timestamps*</br>
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

