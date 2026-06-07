//File:    README.md
//Date:    2026-06-06
//Purpose: Main project README for ts_store

# ts_store — Ultra-Fast, Thread-Safe Event Buffer

**High-throughput in-memory event collection with optional durable asynchronous persistence.**

The hot path (`save_event`) is designed to stay as close to pure in-memory speed as possible while a background thread drains to disk via pluggable sinks (Binary or jText split files today).

---

## What this project is (and is becoming)

ts_store began as a **learning experiment**: C++23, bounded hot-path storage, flags, and persistence sinks explored incrementally. It is growing into a **regression platform** — not finished yet, but aimed at verifying every meaningful change to ts_store before it lands.

The automated matrix (`ts_test_cli`, tests 001–007 TS/XS, flags unit test, dual-compiler smoke) is the proof layer: millions of events, multiple persist modes, per-OS and per-disk result leaves, and promoted summaries under [test-summary/](test-summary/).

**C++23 modules** are used throughout tests and examples. The goal is **not** to ship a reusable SDK for other repos (though the module boundaries could be reused on a similar toolchain). Modules exist so that when you touch one part of ts_store — flags, a sink, the core buffer — **only the affected module units recompile**, keeping iteration time down on a large tree. Tests and examples use `import`; implementation still lives under `include/beman/ts_store/ts_store_headers/` (module `.cppm` files are thin shims — moving bodies into modules is future work).

**New machine or toolchain?** Always do a **clean configure and full build from source** first (`./scripts/build_dual_compilers.sh` or equivalent). Only `modules/**/*.cppm` and companion `.cpp` sources are in git; BMIs and other module build objects (`.gcm`, `.pcm`, `.ddi`, etc.) are gitignored and must be produced locally. Prebuilt module artifacts are not portable across OS, compiler, or CPU. After a good full build, day-to-day work benefits from module granularity on that same system.

---

## Quick Start (Core In-Memory)

```cpp
#include <iostream>

import jac.ts_store.impl.testing;

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
**Typical imports**: `jac.ts_store.impl.testing` (core + test CLI helpers), plus `jac.ts_store.persistence.*` when attaching sinks. Legacy headers remain under [include/beman/ts_store/ts_store_headers/](include/beman/ts_store/ts_store_headers/) for reference; new code in this repo uses modules.

Link the matching CMake targets (e.g. `jac_ts_store_impl_testing`) — see [CMakeLists.txt](CMakeLists.txt) and the [examples/](examples/) targets.

---

## Current Status (mid-2026)

- **Extensively tested** — All stress tests (001–007 TS/XS + flags) exercise the full double-buffered persistence matrix (binary + jText + SQL + pure in-memory) via **module imports**. See [tests/](tests/) and [scripts/ts-test](scripts/ts-test).
- Heavy tests (005/006/007) run at realistic scale (100 threads, 1M events per run, 5 runs for full mode) on spinning rust. Only the last run performs persistence (see the test sources for the exact "last run only" logic).
- Per-OS + per-disk separation: results live under `test-results/OS_00n/<disk>/Smoke|xFull/` and lightweight summaries are promoted to the equivalent path under `test-summary/`. This keeps metrics from different machines and storage types (x7k = 7200 rpm HDD, 10k, ssd) cleanly separated while using short aligned directory names.
- The lightweight summaries are automatically promoted to the tracked `test-summary/` tree so they can be committed as proof without pulling in gigabytes of logs.

The core in-memory path + `DoubleBufferedWriter` + pluggable sinks (JText, Binary, and SQL via `SqlEventSink`) is the primary delivered capability. Advanced query/aggregation beyond `select(id)` remains future work.

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
- `get_test_params()` in [modules/jac.test_framework/runner.cpp](modules/jac.test_framework/runner.cpp) (invoked by `ts_test_cli`)
- Individual heavy test implementations (e.g. [tests/ts_store_005/test_005_TS.cpp](tests/ts_store_005/test_005_TS.cpp))

A 0-int / 0-float variant of the heaviest stress test (005) is also maintained for pure payload + flags pressure testing.

### Flags

A single `uint64_t` carries user hints (`KeeperRecord`, `LogConsole`, `SendNetwork`, `IsResult`, severity, etc.) plus automatic `HasData` / `HasIntData` / `HasDblData` bits.

**Full flag layout and helpers**: [Doc/ts_store_flag_docs.md](Doc/ts_store_flag_docs.md) — module `jac.ts_store.flags`

---

## Persistence (Double-Buffered)

Attach asynchronous persistence without blocking the hot path:

```cpp
import jac.ts_store.impl.testing;
import jac.ts_store.persistence.jtext;

auto sink   = std::make_unique<JTextEventSink>("MyRun", 9, 6);
auto writer = std::make_unique<DoubleBufferedWriter>(std::move(sink), 10'000);
store.attach_persistence(std::move(writer));

auto [ok, id] = store.save_event(...);   // returns instantly
```

Supported today (modules under `modules/jac.ts_store/`):
- `jac.ts_store.persistence.jtext` — `JTextEventSink`, split files (main + _Ints + _Floats)
- `jac.ts_store.persistence.binary` — `BinaryEventSink`, fast length-prefixed mmap path
- `jac.ts_store.persistence.sql` — `SqlEventSink` (when SQLite persist is enabled at configure time)
- `jac.ts_store.persistence.writer` — `DoubleBufferedWriter`

`KeeperRecord` flag controls what must survive to durable storage.

See [examples/](examples/) — all demos and benchmarks use `import`, not raw ts_store headers.

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
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

C++23 modules require **Ninja** (or another generator with `FILE_SET` support). The dual-compiler helper wires this up.

**Compiler requirements:** **GCC 15+** (via `gcc-toolset-15` on RHEL) and **Clang 21+**. Smoke matrix is verified with GCC 15.2.1 and Clang 21.1.8. Older compilers are rejected at configure time when persistence is enabled.

### With jText + SQLite persistence (typical dev / test matrix)
```bash
cmake -G Ninja -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DTS_STORE_ENABLE_SQLITE_PERSIST=ON -DCMAKE_BUILD_TYPE=Debug ..
```

**Vendored mode is the default** — `vendor/jText` and `vendor/jacQlite` are committed so a plain clone builds without siblings. For live cross-project dev, place **`../jText`** and **`../jacQlite`** next to this repo and run `./scripts/build_dual_compilers.sh` (uses reference mode and auto-syncs siblings → `vendor/` after each matrix binary build).

The project is routinely built in several configurations (see the various `build-*` directories in the tree):

- **Local jText** (`build-local-*`): uses a sibling `../jText` checkout (fast iteration during development).
- **Vendored jText** (`build-vendored-*`, `build-dual-vendored`): self-contained using `vendor/jText` (for releases or when you don't want an external jText tree).
- **Results-oriented builds** (`build-results-*`): used by the test matrix for producing artifacts.

Canonical dev trees: `build-dual/gcc` and `build-dual/clang` (jtext+sqlite ON). Use the dual-compiler helper for a clean rebuild:
```bash
./scripts/build_dual_compilers.sh
```

**Build options and jText mode selection** are defined in:
- [CMakeLists.txt](CMakeLists.txt) (see `TS_STORE_ENABLE_JTEXT_PERSIST` and `TS_STORE_JTEXT_MODE`)
- [DUAL_COMPILER_BUILD.md](DUAL_COMPILER_BUILD.md) for manual instructions

Dependency mode at configure time (`TS_STORE_JTEXT_MODE`, `TS_STORE_JACQLITE_MODE`):
- **`vendored`** (default) — `vendor/jText`, `vendor/jacQlite`
- **`reference`** — live siblings `../jText`, `../jacQlite` (used automatically by `build_dual_compilers.sh` when siblings exist)

[scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) copies siblings → `vendor/` (`--sync-all`). Runs automatically after matrix binary builds when siblings are present.

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

See `get_test_params()` / `build_scenario_list()` in [modules/jac.test_framework/runner.cpp](modules/jac.test_framework/runner.cpp), plus the `main()` functions in the heavy test sources (e.g. `tests/ts_store_005/`, `tests/ts_store_007/`). Smoke matrix: **113 scenarios per compiler** (001–007 × TS/XS × 4 persist × 2 output modes + `ts_store_flags`); **226** after gcc+clang merge on the same leaf.

### Important current practices

- `DISK_TYPE` (x7k / 10k / ssd — normalized to exactly 3 characters) keeps results for different storage hardware completely separate.
- The runner **auto-detects** the OS (via `uname` + `/etc/os-release`) and assigns or re-uses a short padded `OS_001` / `OS_002` ... ID. This produces the nested layout `test-results/OS_001/<DISK_TYPE>/<SIZE_LABEL>/` (SIZE_LABEL = "Smoke" or "xFull"). 
  - A visible central list is kept in `test-results/OS_MAP.txt` (e.g. `OS_001 = RHEL 9.6`).
  - Full details for the run (including the assigned ID) are written to `OS_INFO.txt` inside the leaf directory.
  - You can force a specific ID with `OS_ID=OS_001` in params or `--os-id` on the CLI (override kept for debugging, weird containers/WSL/CI, migration, etc.).
  This design keeps all directory names short for perfect vertical alignment (padded OS_00n + 3-char disks x7k/10k/ssd + 5-char sizes) while automatically tracking which real OS each result set came from across machines. The promotion script mirrors the structure under `test-summary/`. (Padded form chosen so the framework can scale if it is successful across many OS variants.)
- `./scripts/run_all_tests.sh` (wrapper) runs gcc+clang sequentially and then `promote_summaries.sh --all`. Direct `ts_test_cli run` does **not** promote — run `./scripts/promote_summaries.sh --all` yourself after a good matrix.

View raw logs (example — current nested layout):
```bash
less -R test-results/OS_003/ssd/Smoke/binary_logs/TS_STORE_TEST_005_TS/gcc_binary_on.log
```

The corresponding summary is at `test-summary/OS_003/ssd/Smoke/README.md` (linked from `test-summary/README.md`).

`test-results/` is fully ignored in `.gitignore`. Only the promoted summaries under `test-summary/` are intended to be tracked.

---

## Examples

Located in `examples/` (many are only built when `TS_STORE_ENABLE_JTEXT_PERSIST=ON`). All ts_store-facing examples use **module imports** and link the corresponding `jac_ts_store_*` CMake targets.

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

- [modules/jac.ts_store/](modules/jac.ts_store/) — C++23 modules (`config`, `flags`, `ansi`, `core`, `impl.testing`, `test_options`, `persistence.{common,binary,jtext,sql,writer}`)
- [modules/jac.test_framework/](modules/jac.test_framework/) + [modules/jac.report/](modules/jac.report/) — matrix runner and summarization
- [include/beman/ts_store/ts_store_headers/](include/beman/ts_store/ts_store_headers/) — implementation headers (included by module units; prefer `import` in application code)
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

- `SqlEventSink` exists and is in the stress matrix, but SQL persistence is optional at configure time and less battle-tested than jText/Binary on every OS leaf.
- No rotation, compaction, or retention policy on the persisted side.
- Query/aggregation beyond `select(id)` is not implemented at runtime.
- Smoke matrix evidence is from **RHEL 9.8 + GCC 15.2.1 + Clang 21.1.8** only. Other platforms need their own run + promote.
- No automated CI yet — regression proof is manual smoke + promoted `test-summary/` commits.

---

## License

See LICENSE file.

**Related**: This work was developed in close collaboration with the [jText](https://github.com/JayACarlsonAtHome/jText) human-readable structured logging library.
