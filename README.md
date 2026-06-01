# ts_store — Ultra-Fast, Thread-Safe Event Buffer

### Why this exists
- Simple, clean user interface — pick it up in minutes</br>
- All the complicated concurrency/details hidden away</br>
- Powerful backend optimized for extreme throughput and correctness</br>

### Current Status
**Extensively tested — zero corruption observed across all runs**</br>  
All stress tests pass 100% on g++ 15 (RHEL 9/10) and recent Clang.</br>

**Not production-ready yet** — API is still evolving (especially around persistence). Double-buffered background writing is in active development.</br>

### Recent Improvements
- Payload and category truncation now UTF-8 aware (truncates on code point boundaries)</br>
- Flag system redesigned with scoped enums for type safety</br>
- Cross-compiler compatibility verified (GCC + Clang)</br>
- Added high-performance persistence layer: human-readable jText split files (`JTextSplitEventLog`) and fast binary path (`BinaryEventLog`) with early header support and auto-batching</br>

### Performance (mid-2026, Release builds)

**Core in-memory performance** (the number that matters for the hot path):
- **16 – 22+ million events per second** on realistic workloads, including 9 integer metrics + 6 double metrics per event.
- This is the speed the in-memory buffer can sustain when persistence is handled asynchronously (the intended model).

**Synchronous persistence throughput** (for reference when using the writers directly):
- **jText** (split human-readable files for debugging): 1.5M – 2.1M+ events/sec depending on payload size and compiler.
- **Binary** (mmap-based fast path): 1.1M – 6.7M+ events/sec, degrades more with larger payloads.

See the detailed **Payload Size Impact** tables below for the full cross-compiler, cross-payload results (80 / 160 / 512 char payloads with 9+6 metrics).

These numbers (and the detailed tables below) were measured using the dedicated benchmark programs:
- `examples/jtext_payload_benchmark.cpp`
- `examples/binary_payload_benchmark.cpp`

You can re-run the full matrix (both compilers, all payload sizes) using `scripts/build_dual_compilers.sh`.

### How to Reproduce These Numbers

To get comparable results on your own machine:

```bash
# 1. Build both GCC (via devtoolset-15) and Clang variants with persistence enabled
./scripts/build_dual_compilers.sh

# 2. Run the payload scaling benchmarks:
./build-dual/gcc/ts_store_jtext_payload_benchmark
./build-dual/gcc/ts_store_binary_payload_benchmark

./build-dual/clang/ts_store_jtext_payload_benchmark
./build-dual/clang/ts_store_binary_payload_benchmark
```

All runs use:
- 300,000 events per run
- 9 integer metrics + 6 double metrics per event
- Payload sizes: 80 / 160 / 512 characters
- Release builds (`-DCMAKE_BUILD_TYPE=Release`)

Results will vary significantly by hardware (especially storage speed for the persistence paths), but the relative performance between jText vs Binary, and between GCC vs Clang, should be reproducible.

These are the exact programs used to generate the numbers in the tables below:
- `examples/jtext_payload_benchmark.cpp`
- `examples/binary_payload_benchmark.cpp`

### Performance Characteristics & Limitations

**Core design principle**: The in-memory path is designed to stay fast (16M+ events/sec) even when persistence is active. This only holds if you use **asynchronous / double-buffered persistence**.

**Synchronous persistence** (calling the writers directly from the hot path) will drop throughput to the 1–7M range depending on payload size and writer (see tables below).

**Important limitations**:
- If you need to persist *every* event synchronously on the critical path, this is a poor fit for true High Frequency Trading (HFT) or other ultra-low-latency systems.
- In any real deployment you will eventually become limited by **storage I/O bandwidth** (disk or network), not by the CPU cost of the writers.

The dedicated benchmark programs make it easy to measure exactly where your hardware hits the IO wall.

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

You can re-run the exact same benchmarks after changes using `scripts/build_dual_compilers.sh`.

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