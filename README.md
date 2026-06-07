//File:    README.md
//Date:    2026-06-06
//Purpose: Main project README for ts_store

# ts_store — Ultra-Fast, Thread-Safe Event Buffer

**High-throughput in-memory event collection with optional durable asynchronous persistence.**

The hot path (`save_event`) is designed to stay as close to pure in-memory speed as possible while a background thread drains to disk via pluggable sinks (Binary or jText split files today).

---

## Quick Start (Core In-Memory)

```cpp
#include <beman/ts_store/ts_store_headers/ts_store.hpp>

using namespace jac::ts_store::inline_v001;

int main() {
    using Config = ts_store_config<true>;   // timestamps enabled
    ts_store<Config> store(8, 10'000);      // 8 threads, 10k events per thread

    auto [ok, id] = store.save_event(
        /*thread_id*/           0,
        /*per_thread_event_id*/ 42,
        /*payload*/             "user logged in",
        /*flags*/               0,
        /*category*/            "AUTH"
    );

    auto [found, payload] = store.select(id);
    if (found) {
        std::cout << payload << '\n';
    }
}
```

**Namespace**: `jac::ts_store::inline_v001`  
**Single include**: `<beman/ts_store/ts_store_headers/ts_store.hpp>`

---

## Current Status (mid-2026)

- **Extensively tested** — All stress tests (001–007 TS/XS + flags) exercise the full double-buffered persistence matrix (binary + jText + SQL roundtrips + pure in-memory) with zero corruption observed across millions of events.
- Heavy tests (005/007) run at realistic scale (250 threads, 1M events, 50 runs for full mode) on spinning rust.
- Per-disk separation: results and summaries are captured under `test-results/<disk>/` and `test-summary/<disk>/` (x7k = 7200 rpm HDD, 10k, ssd) so metrics from different storage types stay cleanly separated.
- The lightweight summaries are automatically promoted to the tracked `test-summary/` tree so they can be committed as proof without pulling in gigabytes of logs.

The core in-memory path + `DoubleBufferedWriter` + pluggable sinks (JTextEventSink / BinaryEventSink) is the primary delivered capability. Direct-to-SQL from the hot path and advanced query features remain future work.

---

## Core Concepts

### The Buffer
- Pre-sized ring: `max_threads × events_per_thread`
- `save_event(...)` returns immediately (lock-free / very low contention hot path)
- `select(id)` returns a `string_view` into the stored payload
- `clear()` is cheap and reuses the buffer

### Configuration
```cpp
using Config = ts_store_config<
    true,      // UseTimestamps
    6,         // MaxTypeLength (reserved)
    20,        // MaxCategoryLength
    512,       // MaxPayloadLength
    9,         // IntMetrics slots (0-9 supported)
    6,         // DblMetrics slots (0-6 supported)
    false,     // EnableMetrics (future)
    false,     // DefaultInteractive
    false,     // DefaultColor
    false      // DebugMode
>;
```

A 0-int / 0-float variant of the heaviest stress test (005) is also maintained for pure payload + flags pressure testing.

### Flags
A single `uint64_t` carries user hints (`KeeperRecord`, `LogConsole`, `SendNetwork`, `IsResult`, severity, etc.) plus automatic `HasData` / `HasIntData` / `HasDblData` bits.

See `Doc/ts_store_flag_docs.md` for the full layout.

---

## Persistence (Double-Buffered)

Attach asynchronous persistence without blocking the hot path:

```cpp
auto sink   = std::make_unique<JTextEventSink>("MyRun", 9, 6);
auto writer = std::make_unique<DoubleBufferedWriter>(std::move(sink), 10'000);
store.attach_persistence(std::move(writer));

auto [ok, id] = store.save_event(...);   // returns instantly
```

Supported today:
- `JTextEventSink` → 3-file split (main + _Ints + _Floats) with includes for shared schemas
- `BinaryEventSink` → fast length-prefixed mmap path
- SQL roundtrips (via external jText CLI tools) are exercised after every jText persist run for verification

`KeeperRecord` flag controls what must survive to durable storage.

See `examples/` for `double_buffered_persistence_demo.cpp`, `ts_store_with_double_buffer_demo.cpp`, and the various throughput benchmarks.

---

## Performance

The number that matters is sustained rate **while durably recording** in the background.

Typical results from the full stress matrix (see the latest per-disk summaries):

- Hot path with async double-buffered jText/Binary: low millions of events/sec on realistic multi-threaded workloads.
- Pure in-memory (no persistence): significantly higher (tens of millions in some cases).

**Primary results documents** (committed, per-disk):

- [test-summary/x7k/TS_STORE_Test_Summary.md](test-summary/x7k/TS_STORE_Test_Summary.md) (and 10k/, ssd/)
- [test-summary/x7k/TS_STORE_InMemory_Summary.md](test-summary/x7k/TS_STORE_InMemory_Summary.md)
- [test-summary/x7k/TS_STORE_SQL_Roundtrip_Summary.md](test-summary/x7k/TS_STORE_SQL_Roundtrip_Summary.md)

Raw logs and persist artifacts live under the (git-ignored) `test-results/<disk>/` tree.

---

## Building

### Basic (in-memory only)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

### With jText Persistence
```bash
cmake -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DCMAKE_BUILD_TYPE=Release ..
```

Use the dual-compiler helper for repeatable GCC 15 + Clang verification:
```bash
./scripts/build_dual_compilers.sh
```

See `DUAL_COMPILER_BUILD.md` for manual instructions.

Two jText modes are supported (controlled by `-DTS_STORE_JTEXT_MODE=`):
- `reference` (default during active development — uses sibling `../jText`)
- `vendored` (uses `vendor/jText` for self-contained / release builds)

---

## Running the Automated Test Suite

All stress tests are driven by `scripts/run_all_tests.sh` + `tests/test_params.txt`.

```bash
# Typical smoke (fast, safe on any disk)
./scripts/run_all_tests.sh

# Full intensity on a specific disk type (HDD recommended for long runs)
# Set in tests/test_params.txt:
#   DISK_TYPE=x7k
#   SIZE=full
#   (checkboxes for which of 001-007 to run)
./scripts/run_all_tests.sh --disk x7k
```

Important current practices:
- `DISK_TYPE` (x7k / 10k / ssd — normalized to exactly 3 characters) keeps results for different storage hardware completely separate.
- The runner **auto-detects** the OS (via `uname` + `/etc/os-release`) and assigns or re-uses a short padded `OS_001` / `OS_002` ... ID. This produces the nested layout `test-results/OS_001/<DISK_TYPE>/<SIZE_LABEL>/` (SIZE_LABEL = "Smoke" or "xFull"). 
  - A visible central list is kept in `test-results/OS_MAP.txt` (e.g. `OS_001 = RHEL 9.6`).
  - Full details for the run (including the assigned ID) are written to `OS_INFO.txt` inside the leaf directory.
  - You can force a specific ID with `OS_ID=OS_001` in params or `--os-id` on the CLI (override kept for debugging, weird containers/WSL/CI, migration, etc.).
  This design keeps all directory names short for perfect vertical alignment (padded OS_00n + 3-char disks x7k/10k/ssd + 5-char sizes) while automatically tracking which real OS each result set came from across machines. The promotion script mirrors the structure under `test-summary/`. (Padded form chosen so the framework can scale if it is successful across many OS variants.)
- `SIZE=full` for 005/007 uses 50 runs of ~1M events each. Only the **last** run of 005 and 007 is verbose on console / test-log.txt; earlier runs still do full work and contribute to statistics.
- Test 005 has a 0-int + 0-float variant (pure payload/flags stress) while 007 retains the full 9/6 metrics.
- After the runner finishes, `scripts/promote_summaries.sh --all` is called automatically. The three `TS_STORE_*_Summary.md` files are copied from `test-results/<disk>/` into `test-summary/<disk>/` (these are the small files you commit).

View raw logs (example):
```bash
less -R test-results/x7k/binary_logs/TS_STORE_TEST_005_TS/gcc.log
```

The corresponding summary is at `test-summary/x7k/TS_STORE_Test_Summary.md`.

`test-results/` is fully ignored in `.gitignore`. Only the promoted summaries under `test-summary/` are intended to be tracked.

---

## Examples

Located in `examples/` (many are only built when `TS_STORE_ENABLE_JTEXT_PERSIST=ON`):

- `ts_store_with_double_buffer_demo.cpp` — recommended integrated usage (`attach_persistence`)
- `double_buffered_persistence_demo.cpp` — standalone DoubleBufferedWriter + sinks
- `jtext_split_persistence_demo.cpp`, `binary_persist_demo.cpp`
- Throughput / payload benchmarks (`*_throughput_test`, `*_payload_benchmark`)
- `jtext_high_throughput_test.cpp`, `in_memory_throughput.cpp`, etc.

Pure binary (no jText) benchmarks are always available.

---

## Project Layout (key parts)

- `include/beman/ts_store/ts_store_headers/` — public + internal headers + persistence
- `tests/ts_store_00N/` — the numbered stress suites (001–007 + flags)
- `tests/test_params.txt` — controls SIZE, DISK_TYPE, and which tests run
- `scripts/run_all_tests.sh` + `promote_summaries.sh` — the automation
- `examples/` — demos and benchmarks
- `tools/jtext_cli/` + `vendor/jText/` — CLI tools for the two jText modes
- `test-summary/<disk>/` — committed lightweight per-disk summaries (x7k/10k/ssd)
- `test-results/<disk>/` — raw logs + artifacts (ignored, large)

---

## Limitations & Future Work

- No native direct-to-SQL writer from the ts_store hot path yet (current SQL work is jText-mediated roundtrips via external CLIs).
- No rotation, compaction, or retention policy on the persisted side.
- Query/aggregation beyond `select(id)` is not implemented at runtime.
- The project has been exercised heavily on one OS/compiler family (RHEL + GCC 15 / recent Clang). Validate on your target platforms.

Native direct SQL (via a pluggable `SqlEventSink` that emits INSERTs straight from the writer) is the main planned persistence addition.

---

## License

See LICENSE file.

**Related**: This work was developed in close collaboration with the [jText](https://github.com/JayACarlsonAtHome/jText) human-readable structured logging library.
