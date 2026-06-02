# ts_store — Ultra-Fast, Thread-Safe Event Buffer

**High-throughput in-memory event collection with optional durable persistence.**

### Why this exists
- Simple, clean API that you can pick up in minutes
- Heavy concurrency and correctness details are completely hidden
- Designed for extreme throughput on the hot path while still offering practical persistence options

---

## Quick Start (Core In-Memory Usage)

```cpp
#include <beman/ts_store/ts_store_headers/ts_store.hpp>

using namespace jac::ts_store::inline_v001;

int main() {
    // 8 threads, 10k events per thread = 80k total capacity
    using Config = ts_store_config<true>;           // timestamps enabled
    ts_store<Config> store(8, 10'000);

    // Save an event (returns immediately, lock-free hot path)
    auto [ok, id] = store.save_event(
        /*thread_id*/           0,
        /*per_thread_event_id*/ 42,
        /*payload*/             "user logged in",
        /*flags*/               0,
        /*category*/            "AUTH"
    );

    // Later: retrieve by stable id
    auto [found, payload] = store.select(id);
    if (found) {
        std::cout << payload << '\n';
    }
}
```

**Namespace**: `jac::ts_store::inline_v001`  
**Single include**: `<beman/ts_store/ts_store_headers/ts_store.hpp>`

---

## Current Status

**Extensively tested** — zero corruption observed across millions of events in stress workloads. All numbered stress tests pass 100% on GCC 15 (RHEL) and recent Clang.

**Not production-ready** — The core in-memory buffer is mature. The persistence layer (especially background/double-buffered draining) is still evolving.

---

## Core Concepts

### The Buffer
- Pre-sized, fixed-capacity ring of events (`max_threads × events_per_thread`)
- `save_event(...)` is the hot path — extremely fast, lock-free on the write side
- `select(id)` gives you a `string_view` into the stored payload
- `clear()` resets the buffer for reuse (very cheap)

### Configuration
```cpp
using Config = ts_store_config<
    true,      // UseTimestamps
    6,         // MaxTypeLength (reserved for future)
    20,        // MaxCategoryLength
    512,       // MaxPayloadLength (UTF-8 safe truncation)
    9,         // IntMetrics slots (0-9 supported in persistence layer)
    6,         // DblMetrics slots (0-6 supported in persistence layer)
    false      // EnableMetrics (future)
>;
```

### Flags (per-event routing & metadata)
A single `uint64_t` carries user routing hints + system metadata:

- `KeeperRecord` (bit 1) — "this one must survive to disk"
- `LogConsole`, `SendNetwork`, `HotCacheHint`, `IsResult`, etc.
- Severity (3 bits)
- Automatic `HasData` / `HasIntData` / `HasDblData` bits set by the store

See `Doc/ts_store_flag_docs.md` for the full layout.

---

## Persistence Layer

Two writers are provided (both support the same early-open + `KeeperRecord` filtering model):

| Writer                  | Purpose                        | Human Readable | Speed (sync)      | Notes |
|-------------------------|--------------------------------|----------------|-------------------|-------|
| `JTextSplitEventLog`    | Debug / inspection / audit     | Yes (3 files)  | Very good         | Main + `_Ints.jtext` + `_Floats.jtext` with linking IDs |
| `BinaryEventLog`        | Production "blazing fast" path | No             | Fastest           | Length-prefixed mmap writer |

**Both writers are opt-in.**

### Enabling Persistence

```bash
cmake -DTS_STORE_ENABLE_JTEXT_PERSIST=ON ..
```

Requirements when enabled:
- Sibling checkout of `../jText` (the jText library this project was built with)
- GCC 13+ or Clang 16+ (GCC 15 via devtoolset-15 and Clang 21 tested)

The persistence headers live under `include/beman/ts_store/ts_store_headers/persistence/`.

Example usage (jText split files with 9 ints + 6 doubles):

```cpp
#include <beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.hpp>

JTextSplitEventLog log("MyRun", 9, 6);   // early open + headers written here

log.append_event(event_id, thread_id, per_thread_id, flags,
                 category, payload, timestamp,
                 int_vector, double_vector);

log.flush();
log.finalize();   // or let destructor do it
```

The same pattern works with `BinaryEventLog`.

---

## Performance Reality (mid-2026)

### In-Memory Hot Path (the number that actually matters)
The core `save_event` path routinely achieves **16–22+ million events/sec** on realistic workloads (9 integer metrics + 6 double metrics + payload + flags) when persistence is drained asynchronously.

This is the design target. The entire architecture (double-buffering intent, etc.) exists to keep the hot path at this speed.

### Synchronous Persistence Throughput
When you call the writers directly from the hot path:

- `JTextSplitEventLog` (with 10K auto-batching): typically in the **1.5M – 2.5M+ events/sec** range depending on payload size and compiler.
- `BinaryEventLog` (mmap): often faster on small payloads, degrades more as payload grows.

**Critical caveat**: If you need to persist *every* event synchronously on the critical path, this design is a poor fit for true HFT or other ultra-low-latency systems. The in-memory core is fast; durable I/O is not.

The intended model is **asynchronous / double-buffered draining** — the hot path stays in memory at full speed while a background thread writes.

In any real deployment you will eventually become limited by storage I/O bandwidth, not CPU.

### Reproducing Numbers

The dedicated benchmark programs live in `examples/`:

```bash
# Recommended: build both compilers cleanly
./scripts/build_dual_compilers.sh

# Then run (example paths after dual build)
./build-dual/gcc/ts_store_jtext_payload_benchmark
./build-dual/gcc/ts_store_binary_payload_benchmark
```

These programs measure **direct writer throughput** (not the core in-memory path).

---

## Build Instructions

### Normal (in-memory only, no persistence)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

### With jText Persistence Enabled
```bash
cmake -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DCMAKE_BUILD_TYPE=Release ..
```

Use the provided script for repeatable dual-compiler verification:
```bash
./scripts/build_dual_compilers.sh
```

See `DUAL_COMPILER_BUILD.md` for manual GCC 15 + Clang instructions.

---

## Examples Directory

Located in `examples/`:

- `jtext_split_persistence_demo.cpp` — Full early-open + 3-file jText split with metrics + KeeperRecord filtering
- `binary_persist_demo.cpp` — Side-by-side jText vs Binary comparison
- `jtext_payload_benchmark.cpp` / `binary_payload_benchmark.cpp` — The programs behind the performance numbers
- `Event_and_Summary_*.cpp` — Save + select + result aggregation pattern (derived from older stress tests)

---

## Tests Directory

`tests/ts_store_00N/` contain the historical numbered stress suites (001–007, each with `_TS` timestamps and `_XS` no-timestamps variants).

These are **not introductory examples**. They are heavy multi-threaded corruption and throughput verification harnesses that use internal test-only APIs (`verify_level01`, `diagnose_failures`, etc.). They compile with `TS_STORE_ENABLE_TEST_CHECKS`.

They remain useful for regression testing but are not the recommended on-ramp for new users.

### Automated Test Runner & Results

All 15 stress tests (001–007 TS + XS variants, plus the flags test) can be run via the automation script:

```bash
./scripts/run_all_tests.sh --compiler gcc --output no
./scripts/run_all_tests.sh --compiler clang --output yes
```

- `--compiler gcc|clang` selects the toolchain (GCC uses `scl enable gcc-toolset-15`).
- `--output yes|no` selects console output (tee to stdout + log) or silent (logs only). This is the "controls selector" for output/no-output.
- The script ensures binaries are built (using the dual-compiler pattern), runs every test, logs full output per test to `results/<compiler>/`, and generates a `summary.md` page.

**Results pages** (generated by the runner, all tests pass with 100% verification):

- [GCC 15 (gcc-toolset-15) Results](results/gcc/summary.md)
- [Clang 21 Results](results/clang/summary.md)

The runner also supports the double-buffered configuration for the large tests (005/007) as updated previously.

See `scripts/run_all_tests.sh` for the implementation (pure automation, no external deps beyond the built test binaries).

---

## Design History & Rationale

Every significant decision (persistence split-file format, jText dependency strategy, KeeperRecord semantics, build opt-in, binary format choice, etc.) is documented in the `Rationale/` folder with numbered decision records.

---

## Current Limitations & In-Progress Work

- Query/aggregation features beyond `select(id)` are not implemented
- No rotation, compaction, or long-term storage policy yet
- (Double-buffered persistence is fully wired and exercised in the automated stress tests — see the results pages and Rationale/10_...)

---

## Planned

- Double-buffered background IO — asynchronous draining from the in-memory hot path to the persistence writers (**implemented** — see below)
- jText → SQL pathway in the future (easy extraction / loading of the human-readable split files into relational storage when needed)
- Direct ts_store → SQL writer option (as a future alternative or complement to the current jText and Binary persistence layers)

### Double-Buffered Persistence (New)

You can now attach asynchronous persistence directly to a `ts_store`:

```cpp
using Config = ts_store_config<true, 6, 32, 256, 9, 6>;
ts_store<Config> store(8, 10'000);

auto sink   = std::make_unique<JTextEventSink>("MyRun", 9, 6);
auto writer = std::make_unique<DoubleBufferedWriter>(std::move(sink), 10'000);

store.attach_persistence(std::move(writer));

// Normal usage — persistence happens in the background
auto [ok, id] = store.save_event(thread, per_thread, payload, flags, category, true, ints, dbls);
```

The same pattern works with `BinaryEventSink`. Future SQL sinks will plug in identically.

See:
- `examples/ts_store_with_double_buffer_demo.cpp` (integrated)
- `examples/double_buffered_persistence_demo.cpp` (standalone)
- `Rationale/10_Double_Buffered_Pluggable_Persistence.md`

---

## There Are No Guarantees

This code has been tested extensively on one platform and compiler family. Make and run your own validation tests before relying on it.

The API (especially around persistence) is still allowed to change.

---

**License**: See LICENSE file.

**Related projects**: This work was done in close collaboration with the [jText](https://github.com/JayACarlsonAtHome/jText) human-readable structured logging library.