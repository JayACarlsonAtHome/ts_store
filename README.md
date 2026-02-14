# ts_store — Ultra-Fast, Thread-Safe Event Buffer

### Why this exists
- Simple, clean user interface — pick it up in minutes</br>
- All the complicated concurrency/details hidden away</br>
- Powerful backend optimized for extreme throughput and correctness</br>

### Current Status — February 2026
**Extensively tested — zero corruption observed across all runs**</br>  
All 10 stress tests pass 100% on g++ 15.1.1 (RHEL 9/10) and Clang.</br>

**Not production-ready yet** — API is in flux and will change before final lock-down.</br>

### Recent Improvements
- Payload and category truncation now UTF-8 aware (truncates on code point boundaries)</br>
- Flag system redesigned with scoped enums for type safety</br>
- Cross-compiler compatibility verified (GCC + Clang)</br>

### Performance (measured 2026-01-12)</br>
<img width="662" height="135" alt="image" src="https://github.com/user-attachments/assets/b64778fc-c8b5-4a18-99ca-55077a73b818" />
</br>
### Performance (measured 2026-01-26) -- Fastest Run in a while</br>
<img width="662" height="117" alt="image" src="https://github.com/user-attachments/assets/1a660830-217b-4007-a7a2-8cfa6f75f543" />
</br>
### Performance (measured 2026-01-31)</br>
<img width="662" height="117" alt="image" src="https://github.com/user-attachments/assets/e2c3b2ee-f676-4458-a479-46655a3f6298" />
</br>
*Writes: 250 threads × 4000 events (1,000,000) over 50-1000 runs (real data from test_005).*</br>
*Average ~15-16 million operations per second with or without timestamps*</br>
*Tests run on GCC 15.1.1; recent builds also verified on Clang.*</br>

### Planned Features — Q1/Q2 2026
- Double-buffered disk persistence</br>
- Fast queries/filtering by Type, Category, or Payload</br>
- Numeric values in payloads with math operations (sum, min, max, avg, etc.)</br>
- Rollups/aggregates over Type, Category, or global</br>

### Note
Recent additions (UTF-8 truncation, enum flags) maintain performance while improving safety.</br>  
Future optimization: consider fixed-size buffers for payloads to preserve SSO benefits.</br>

### There are no guarantees on this code.
Tested primarily on one platform; other environments may vary.</br>
Make and run your own validation tests.</br>

### Usage Examples
See the test files:</br>
test_001... → very simple example</br>
test_001_TS.cpp through test_005_TS.cpp → Timestamps enabled</br>
test_001_XS.cpp through test_005_XS.cpp → Timestamps disabled</br>
test_005... → substantial numbers</br>