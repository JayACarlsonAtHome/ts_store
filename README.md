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

- **Extensively tested** — All stress tests (001–007 TS/XS + flags) exercise the full double-buffered persistence matrix (binary + jText + SQL roundtrips + pure in-memory) with zero corruption observed across millions of events. See [tests/](tests/) and [scripts/run_all_tests.sh](scripts/run_all_tests.sh).
- Heavy tests (005/006/007) run at realistic scale (100 threads, 1M events per run, 5 runs for full mode) on spinning rust. Only the last run performs persistence (see the test sources for the exact "last run only" logic).
- Per-OS + per-disk separation: results live under `test-results/OS_00n/<disk>/Smoke|xFull/` and lightweight summaries are promoted to the equivalent path under `test-summary/`. This keeps metrics from different machines and storage types (x7k = 7200 rpm HDD, 10k, ssd) cleanly separated while using short aligned directory names.
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

The main compile-time configuration is done via the `ts_store_config` template:

```cpp
using Config = ts_store_config<true /*UseTimestamps*/>;
ts_store<Config> store(8, 10'000);
```

**Full template parameters and documentation** (including `bounded_string` storage, metric slots, etc.):

- [ts_store_config.hpp](include/beman/ts_store/ts_store_headers/ts_store_config.hpp)

Test-specific runtime sizing (threads, events, runs, and the "progressive difficulty" + "only last run persists" rules for 005/006/007) is controlled by:

- [tests/test_params.txt](tests/test_params.txt)
- The `get_test_params()` function and per-test logic in [scripts/run_all_tests.sh](scripts/run_all_tests.sh)
- Individual heavy test implementations (e.g. [tests/ts_store_005/test_005_TS.cpp](tests/ts_store_005/test_005_TS.cpp))

A 0-int / 0-float variant of the heaviest stress test (005) is also maintained for pure payload + flags pressure testing.

### Flags

A single `uint64_t` carries user hints (`KeeperRecord`, `LogConsole`, `SendNetwork`, `IsResult`, severity, etc.) plus automatic `HasData` / `HasIntData` / `HasDblData` bits.

**Full flag layout and helpers**:
- [Doc/ts_store_flag_docs.md](Doc/ts_store_flag_docs.md)
- Flag utilities are defined alongside the main headers (see `ts_store.hpp` and related impl_details).

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
- [JTextEventSink](include/beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp) → 3-file split (main + _Ints + _Floats) with includes for shared schemas
- [BinaryEventSink](include/beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp) → fast length-prefixed mmap path
- SQL roundtrips (via external jText CLI tools) are exercised after every jText persist run for verification

`KeeperRecord` flag controls what must survive to durable storage.

See [examples/](examples/) for `double_buffered_persistence_demo.cpp`, `ts_store_with_double_buffer_demo.cpp`, and the various throughput benchmarks. Also see the `DoubleBufferedWriter` header for the attachment API.

---

## Performance

The number that matters is sustained rate **while durably recording** in the background.

Typical results from the full stress matrix (see the latest summaries under `test-summary/`, generated by the runner):

- Hot path with async double-buffered jText/Binary: low millions of events/sec on realistic multi-threaded workloads.
- Pure in-memory (no persistence): significantly higher (tens of millions in some cases).

**Primary results documents** (committed, per-OS + per-disk):

The test framework uses an `OS_00n` / disk / size layout. Start at the hub:

- [test-summary/README.md](test-summary/README.md) — index of all promoted runs (`{OS}/{disk}/{Smoke|xFull}`)
- Example leaf: [test-summary/OS_003/ssd/Smoke/README.md](test-summary/OS_003/ssd/Smoke/README.md) — per-test links under `by_test/`

Each leaf includes `run_manifest.jtext` (machine-readable matrix), `README.md` (navigation), and `by_test/*.md` (per-binary detail).

Raw logs, .jtext/.bin artifacts, and per-run `.meta` files live under the (git-ignored) `test-results/` tree. Only the small promoted tree under `test-summary/` is intended to be committed.

---

## Building

### Basic (in-memory only)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

### With jText Persistence Enabled
```bash
cmake -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DCMAKE_BUILD_TYPE=Release ..
```

The project is routinely built in several configurations (see the various `build-*` directories in the tree):

- **Local jText** (`build-local-*`): uses a sibling `../jText` checkout (fast iteration during development).
- **Vendored jText** (`build-vendored-*`, `build-dual-vendored`): self-contained using `vendor/jText` (for releases or when you don't want an external jText tree).
- **Results-oriented builds** (`build-results-*`): used by the test matrix for producing artifacts.

Use the dual-compiler helper for repeatable GCC 15 + recent Clang verification:
```bash
./scripts/build_dual_compilers.sh
```

**Build options and jText mode selection** are defined in:
- [CMakeLists.txt](CMakeLists.txt) (see `TS_STORE_ENABLE_JTEXT_PERSIST` and `TS_STORE_JTEXT_MODE`)
- [DUAL_COMPILER_BUILD.md](DUAL_COMPILER_BUILD.md) for manual instructions

jText mode is selected at configure time with:
- `-DTS_STORE_JTEXT_MODE=reference` (default for development — expects sibling `../jText`)
- `-DTS_STORE_JTEXT_MODE=vendored` (uses the copy in `vendor/jText`)

The [scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) script can be used to update the vendored copy when needed.

---

## Running the Automated Test Suite

All stress tests are driven by the new **C++ CLI tool**:

- [tools/test_cli/main.cpp](tools/test_cli/main.cpp) built as `ts_test_cli`
- Wrapper: [scripts/ts-test](scripts/ts-test)
- [tests/test_params.txt](tests/test_params.txt) — controls `SIZE`, `DISK_TYPE`, selected tests, and optional `OS_ID`

This replaces the previous Python and shell versions for much better error handling, type safety, and consistency with the rest of the C++ project.

```bash
# Using the new C++ CLI (recommended)
./scripts/ts-test run

# Or directly from your build directory after `cmake --build .`
./ts_test_cli run --disk x7k

# Full intensity
# Set in tests/test_params.txt: SIZE=full, DISK_TYPE=...
./scripts/ts-test run --disk x7k
```

**Key behaviors are implemented in the runner + test sources** (not duplicated here):
- Progressive sizing (001–004 stay small; 005/006/007 capped at 1M events/run)
- "Only the last run performs persistence" for 005/006/007 (earlier runs are pure hot-path measurement)
- OS auto-detection + `OS_00n` layout
- Promotion of lightweight summaries

See the comments and `get_test_params()` inside the runner script, plus the `main()` functions in the heavy test sources (e.g. `tests/ts_store_005/` and `tests/ts_store_007/`) for the exact current rules.

### Important current practices

- `DISK_TYPE` (x7k / 10k / ssd — normalized to exactly 3 characters) keeps results for different storage hardware completely separate.
- The runner **auto-detects** the OS (via `uname` + `/etc/os-release`) and assigns or re-uses a short padded `OS_001` / `OS_002` ... ID. This produces the nested layout `test-results/OS_001/<DISK_TYPE>/<SIZE_LABEL>/` (SIZE_LABEL = "Smoke" or "xFull"). 
  - A visible central list is kept in `test-results/OS_MAP.txt` (e.g. `OS_001 = RHEL 9.6`).
  - Full details for the run (including the assigned ID) are written to `OS_INFO.txt` inside the leaf directory.
  - You can force a specific ID with `OS_ID=OS_001` in params or `--os-id` on the CLI (override kept for debugging, weird containers/WSL/CI, migration, etc.).
  This design keeps all directory names short for perfect vertical alignment (padded OS_00n + 3-char disks x7k/10k/ssd + 5-char sizes) while automatically tracking which real OS each result set came from across machines. The promotion script mirrors the structure under `test-summary/`. (Padded form chosen so the framework can scale if it is successful across many OS variants.)
- After the runner finishes, `scripts/promote_summaries.sh --all` is called automatically. It copies `run_manifest.jtext`, `README.md`, and `by_test/*.md` into `test-summary/` and regenerates the hub index.

View raw logs (example — current nested layout):
```bash
less -R test-results/OS_003/ssd/Smoke/binary_logs/TS_STORE_TEST_005_TS/gcc.log
```

The corresponding summary is at `test-summary/OS_003/ssd/Smoke/README.md` (linked from `test-summary/README.md`).

`test-results/` is fully ignored in `.gitignore`. Only the promoted summaries under `test-summary/` are intended to be tracked.

---

## Examples

Located in `examples/` (many are only built when `TS_STORE_ENABLE_JTEXT_PERSIST=ON`):

**Recommended usage**
- `ts_store_with_double_buffer_demo.cpp` — recommended integrated usage with `attach_persistence`
- `double_buffered_persistence_demo.cpp` — standalone `DoubleBufferedWriter` + sinks

**Persistence demos**
- `jtext_split_persistence_demo.cpp` — jText split-file persistence (main + _Ints + _Floats)
- `binary_persist_demo.cpp` — fast binary persistence
- `binary_payload_benchmark.cpp`, `jtext_payload_benchmark.cpp`

**Throughput & stress**
- `binary_throughput_test.cpp`, `jtext_throughput_test.cpp`
- `jtext_high_throughput_test.cpp`, `in_memory_throughput.cpp`
- `binary_persist_demo.cpp` (also used for persistence throughput)

**Utilities**
- `slurp_jtext_to_sqlite.cpp` — example of loading jText output into SQLite

Pure binary (no jText) examples and benchmarks are always available even without the jText option. Many of the heavy stress tests (especially 005/007) are excellent real-world usage examples.

---

## Project Layout (key parts)

- [include/beman/ts_store/ts_store_headers/](include/beman/ts_store/ts_store_headers/) — public headers + internal implementation + all persistence sinks (see `ts_store_config.hpp`, `ts_store.hpp`, and `persistence/`)
- [tests/ts_store_00N/](tests/) — the numbered stress test suites (001–007 TS/XS + flags unit test)
- [tests/test_params.txt](tests/test_params.txt) — controls `SIZE` (smoke/full), `DISK_TYPE`, selected tests, and `OS_ID`
- [scripts/ts-test](scripts/ts-test) — thin wrapper around the C++ `ts_test_cli` tool
- Legacy: [scripts/run_all_tests.sh](scripts/run_all_tests.sh) (still works)
- [scripts/promote_summaries.sh](scripts/promote_summaries.sh) — copies lightweight summaries out of `test-results/` into trackable `test-summary/`
- [scripts/build_dual_compilers.sh](scripts/build_dual_compilers.sh) + [scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) — build & vendoring helpers
- [examples/](examples/) — demos, throughput benchmarks, and persistence examples
- [tools/jtext_cli/](tools/jtext_cli/) — jText CLI tools (process / retrieve)
- [tools/test_cli/](tools/test_cli/) — C++ test matrix CLI tool (`ts_test_cli` - the recommended way to drive the full test suite)
- [vendor/jText/](vendor/jText/) — vendored copy of the jText library
- [test-summary/](test-summary/) — committed lightweight per-OS/per-disk summaries (under `OS_00n/<disk>/Smoke|xFull/`)
- `test-results/` — raw logs, .jtext/.bin artifacts, .meta files (**git-ignored**, very large)

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
