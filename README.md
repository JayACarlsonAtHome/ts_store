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

### Performance Characteristics & Limitations

**In-memory hot path**: ~15–18 million events/sec (pure `save_event` calls, no I/O).

**Disk persistence (jText)**: Currently in the 900k – 1.6M events/sec range when writing synchronously with 10K auto-batching (human-readable debug path).

**Disk persistence (Binary)**: Fast binary path (length-prefixed records, mmap writer). Early measurements show 1.1M – 1.4M+ events/sec depending on payload size and buffer configuration. This is the production fast path when human readability is not required.

**Important caveat**:  
If you need to persist *every* event synchronously on the critical path, this design will be a poor fit for true High Frequency Trading (HFT) or other ultra-low-latency systems. The in-memory core is fast, but writing to disk (even batched) is orders of magnitude slower than pure memory operations.

The intended usage model is **asynchronous / double-buffered persistence** — the hot path stays in-memory at full speed while events are drained in the background.

In real deployments you will eventually become limited by storage I/O (or network if writing remotely), not by the writer itself.

### Payload Size Impact (9 ints + 6 doubles, 300k events, Release builds)

**GCC 15.2 (gcc-toolset-15)**

| Payload Size | jText Throughput     | Binary Throughput    |
|--------------|----------------------|----------------------|
| 80 chars     | **19.50M** /sec      | 6.73M /sec           |
| 160 chars    | **19.70M** /sec      | 6.54M /sec           |
| 512 chars    | **21.69M** /sec      | 3.86M /sec           |

**Clang 21.1**

| Payload Size | jText Throughput     | Binary Throughput    |
|--------------|----------------------|----------------------|
| 80 chars     | **21.37M** /sec      | 6.48M /sec           |
| 160 chars    | **21.38M** /sec      | 6.30M /sec           |
| 512 chars    | **28.72M** /sec      | 3.78M /sec           |

**Observations**:
- On these workloads, the optimized jText path is significantly faster than the current binary implementation.
- jText throughput is very stable (or even improves slightly) as payload size grows, while the binary path degrades noticeably at 512 bytes.
- **In real life, you will eventually become limited by storage I/O bandwidth** (or network if writing remotely), not by the writer CPU cost. At sufficiently high event rates or payload sizes, the disk becomes the real bottleneck.
- Clang 21 shows a noticeable advantage over GCC 15 for the jText path on larger payloads in these tests.

These numbers were produced by the dedicated benchmarks:
- `examples/jtext_payload_benchmark.cpp`
- `examples/binary_payload_benchmark.cpp`

You can re-run them easily after changes (see `scripts/build_dual_compilers.sh` for building both compiler variants).

### Persistence Options

- **jText** (`JTextSplitEventLog`): Human-readable split files (main + ints + floats). Excellent for headless debugging and manual inspection with vim/gtext. Slower but very useful when you have nothing else.
- **Binary** (`BinaryEventLog`): High-speed length-prefixed binary format. This is the production / "blazing fast" path. Not meant for direct human reading. Use this when you need maximum throughput.

### Planned Features — Q1/Q2 2026
- **jText-based disk persistence** (currently in active development — synchronous version available, double-buffering in progress)</br>
- True double-buffered background persistence (non-blocking hot path)</br>
- Fast queries/filtering by Type, Category, or Payload</br>
- Numeric values in payloads with math operations (sum, min, max, avg, etc.)</br>
- Rollups/aggregates over Type, Category, or global</br>

### Note
Recent additions (UTF-8 truncation, enum flags) maintain performance while improving safety.</br>  
See the **Performance Characteristics & Limitations** section above for details on in-memory vs. persistence throughput.

### There are no guarantees on this code.
Tested primarily on one platform; other environments may vary.</br>
Make and run your own validation tests.</br>

### Usage Examples
See the test files:</br>
test_001... → very simple example</br>
test_001_TS.cpp through test_005_TS.cpp → Timestamps enabled</br>
test_001_XS.cpp through test_005_XS.cpp → Timestamps disabled</br>
test_005... → substantial numbers</br>