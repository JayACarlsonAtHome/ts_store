//File:    /home/jay/git/ts_store/README.md
//Date:    2026-06-05
//Purpose: README for ts_store Library
//
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

**Not production-ready** — The core in-memory buffer is mature. The persistence layer (double-buffered async draining to Binary/jText) is now exercised in all stress tests with measurable results (see TS_STORE_Test_Summary.md). jText persist runs are automatically followed by CLI-based roundtrips to SQL (jtext_process emit + jtext_to_sqlite direct load + queries + sqlite_to_jtext export back); see TS_STORE_SQL_Roundtrip_Summary.md. The on-disk formats and query story continue to evolve.

---

## Core Concepts

### The Buffer
- Pre-sized, fixed-capacity ring of events (`max_threads × events_per_thread`)
- `save_event(...)` is the hot path — fast in-memory operation; with async double-buffered persistence the measured throughput on realistic multi-threaded workloads (e.g. 005/007 stress tests) is in the low millions of events/sec (see Performance section and `TS_STORE_Test_Summary.md`)
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
    false,     // EnableMetrics (future)
    false,     // DefaultInteractive (CLI overrides; default off for tests)
    false,     // DefaultColor (CLI overrides; default off for tests)
    false      // DebugMode
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

### In-Memory Hot Path with Asynchronous Persistence

The core `save_event` + `submit_event` path is the critical hot path. The entire double-buffering and pluggable persistence architecture exists to keep this path as fast as possible while still providing durable, asynchronous recording of events.

In the high-concurrency stress tests that represent realistic workloads (9 integer metrics + 6 double metrics + payload + flags, 250 threads, full asynchronous persistence via `DoubleBufferedWriter` + sink):

- The measured hot-path throughput is typically **2–2.5 million events/sec** (see the latest numbers in `TS_STORE_Test_Summary.md` for the 005/007 runs).

Higher rates (up to ~29M events/sec in some configurations) have been observed, particularly when persistence is not attached (pure in-memory only) or under much lower contention. Without actually recording the data, it is trivial to achieve very high artificial rates (a simple counter increment can do hundreds of millions per second). The number that matters is what the hot path can sustain *while* durably persisting events asynchronously in the background.

This remains the design target: keep the in-memory hot path as close as possible to its maximum speed even with real persistence happening asynchronously.

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

The full `run_all_tests.sh` now also runs pure in-memory versions of the heavy 005/007 tests (with `--persist=none`) and produces a separate `TS_STORE_InMemory_Summary.md` with compile times + hot path rates (no logging).

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
./scripts/run_all_tests.sh                  # runs everything for gcc + clang (default)
./scripts/run_all_tests.sh --compiler gcc --output yes
./scripts/run_all_tests.sh --compiler clang --output yes
# Use --output no for either if you don't want the live test output printed to your console (only meta messages + full logs).
```

- No `--compiler` (or `--compiler all`) runs the full suite for both gcc and clang internally (120 scenarios total).
- `--compiler gcc|clang` selects just one toolchain (GCC uses `scl enable gcc-toolset-15`).
- `--output yes|no` selects live console output (with ANSI colors) or silent (logs only).
- Every test now exercises the persistence matrix as scenarios that must be run and documented:
  - binary logs
  - jtext logs
  - straight to SQL (direct via SqlEventSink writing INSERTs to DB; for debug also writes the Insert statements to a .sql file)
  - inMemory - no persistence (--persist=none)
  - Round tripping tests (DB to jtext, jtext to DB) after the relevant persist runs.
- Summaries, READMEs, other documentation and Rationals updated for all.
- --persist= controls it in the runner (and --base-name for output location).
- Test selection and sizes controlled by `tests/test_params.txt` ( [x] which tests, SIZE=smoke for ~100 records or full for high intensity). This avoids SSD wear for routine runs; smoke is default for most, full only when needed. Runner passes --threads --events-per-thread --runs to tests.

Combinatorial fields (product = scenario count):
  1. Compiler   : gcc + clang (when default/all)
  2. Test       : 15 variants (001_TS..007_XS + flags)
  3. PersistType: binary, jtext, sql (direct-to-SQL with debug INSERT file), none (pure in-memory)
  4. OutputMode : on (live + ANSI colors), off (silent)
     (table sorted Test + PersistType + Output(on before off) + Compiler for easy comparison)

Per compiler: 15 × 4 × 2 = 120 scenarios. Full run: 240 total.
Additionally, roundtrip verification (jText<->SQL, DB<->jText) is performed after the relevant persist runs.
- Structured output (runner logs + the actual persist artifacts `.bin` / `.jtext*`) goes to `test-results/<DISK_TYPE>/binary_logs/TS_STORE_TEST_00N_XX/` and `test-results/<DISK_TYPE>/jText_logs/TS_STORE_TEST_00N_XX/`. (DISK_TYPE e.g. x7k/10k/ssd set in test_params.txt or via --disk; accepts 7k/7200/x7k/10k etc. and normalizes to x7k/10k/ssd (all exactly 3 chars) for alignment)
- A rich summary (with OS, compiler column, per-compiler times (build + test suite per compiler), total suite duration, per-run durations, record counts, "which log was faster", Config settings, etc.) is generated at the project root.

**Primary results document** (default `./scripts/run_all_tests.sh` runs both gcc and clang to get full cross-compiler + binary-vs-jText data):

- [TS_STORE_Test_Summary.md](TS_STORE_Test_Summary.md) (full runs with persistence)
- [TS_STORE_InMemory_Summary.md](TS_STORE_InMemory_Summary.md) (pure in-memory hot path, no logs)
- [TS_STORE_SQL_Roundtrip_Summary.md](TS_STORE_SQL_Roundtrip_Summary.md) (jText → SQL direct via CLIs + export-back for every jtext scenario; sizes + timings co-located with jText logs)

See `scripts/run_all_tests.sh` for implementation details (default runs both compilers). The old `results/<compiler>/` tree is legacy (new output lives under `test-results/<DISK_TYPE>/` with dirs x7k/10k/ssd (all exactly 3 chars) that line up vertically).

The runner always passes `--interactive=0 --persist=... --base-name=...`.
- For `--output yes` (live): uses `--color=1` so the console gets ANSI colors (pretty tables, headers, etc.).
- For `--output no` (silent): uses `--color=0`.
Captured `.log` files are always stripped of ANSI for cleanliness.
You get pretty colors on live runs; direct execution of a test binary also respects `--color`.

To view a captured log: `less -R test-results/x7k/binary_logs/TS_STORE_TEST_003_TS/gcc.log` (or the equivalent under `jText_logs/`). (use x7k/10k/ssd or --disk x7k; 7k/7200 etc. also accepted and normalized)

### jText-to-SQL Roundtrips (via CLI tools + export back)

**Clarification** (per note): This is *jText-mediated* SQL roundtrips via external CLI tools. It is **not** native "direct to SQL" from the ts_store executable.

A true direct-to-SQL persist path would be the ts_store (via a pluggable SqlEventSink attached with DoubleBufferedWriter) emitting INSERT statements or using prepared statements directly to a database — without going through jText files as an interchange step.

What the automated `run_all_tests.sh` currently performs after every jtext persist scenario (on and off), as an interim verification step to exercise SQL loading + roundtrips:

- `jtext_process` CLI emits the `.sql` companions (CREATE + INSERT OR IGNORE templates substituted from the jtext data; canonical `//File: //Date: //Purpose: //Related:` headers).
- `jtext_to_sqlite` (from jacQLite) loads the jText data into a temp SQLite DB (jText → SQL path using the interchange format + tools).
- Simple COUNT(*) queries are run against the loaded tables (main + ints + floats splits).
- `sqlite_to_jtext` CLI exports the DB content back out to `<base>_fulltrip/` directories (jText files + companions) for roundtrip fidelity verification.

Artifacts (`.sql`, `*_fulltrip/`) are co-located with the source jText logs under `test-results/<DISK_TYPE>/jText_logs/TS_STORE_TEST_.../` so they are included for size and timing tracking alongside the binary/jtext files.

A dedicated structured summary is produced:

- [TS_STORE_SQL_Roundtrip_Summary.md](TS_STORE_SQL_Roundtrip_Summary.md) — per-compiler, per-test post-processing times, .sql sizes, fulltrip sizes, loaded row counts, etc. (regional-aware numbers, accurate scale from the actual run).

All of this uses the published CLI tools only (no hard-coded parser logic inside the runner or ts_store core). The test binaries themselves continue to use `--persist binary|jtext|none`.

Native direct writing of INSERTs from ts_store is still future/planned work (see "Planned" below). See the runner source and the SQL summary for the exact captured fields.

---

## Design History & Rationale

Every significant decision (persistence split-file format, jText dependency strategy, KeeperRecord semantics, build opt-in, binary format choice, etc.) is documented in the `Rationale/` folder with numbered decision records.

---

## Current Limitations & In-Progress Work

- Query/aggregation features beyond `select(id)` are not implemented (runtime); offline jText → SQL roundtrips via CLI tools (load + queries + export back) are now automatically exercised after every jtext persist run and summarized (see TS_STORE_SQL_Roundtrip_Summary.md and README "jText-to-SQL Roundtrips" section). Native direct-to-SQL (INSERTs written straight from ts_store) remains future work.
- No rotation, compaction, or long-term storage policy yet
- (Double-buffered persistence with Binary + jText sinks is now exercised by *all* automated stress tests via the runner — see TS_STORE_Test_Summary.md and Rationale/10_...)

---

## Planned

- (jText → SQL roundtrips via CLI tools are now exercised automatically after every jtext persist scenario — see "jText-to-SQL Roundtrips" section and TS_STORE_SQL_Roundtrip_Summary.md)
- Native direct ts_store → SQL writer (INSERTs / prepared statements straight from the exe or a SqlEventSink, without jText interchange) as a pluggable persistence option (future)

**Double-buffered background IO** (asynchronous draining) is **implemented** (see the "Double-Buffered Persistence" section below and the automated stress tests).

### Double-Buffered Persistence (New)

You can now attach asynchronous persistence directly to a `ts_store`:

```cpp
using Config = ts_store_config<true, 6, 32, 256, 9, 6, false, false, false, false>;
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