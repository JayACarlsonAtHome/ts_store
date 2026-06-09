//File:    README.md
//Date:    2026-06-06
//Purpose: Main project README for ts_store

# ts_store — Ultra-Fast, Thread-Safe Event Buffer

**High-throughput in-memory event collection with optional durable asynchronous persistence.**

The hot path (`save_event`) is designed to stay as close to pure in-memory speed as possible while a background thread drains to disk via pluggable sinks (Binary or jText split files today).

**Structure & architecture:** [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md) — system layers, module graph, repo layout, test matrix, and build workflow.

---

## What this project is (and is becoming)

ts_store began as a **learning experiment**: C++23, bounded hot-path storage, flags, and persistence sinks explored incrementally. It is growing into a **regression platform** — not finished yet, but aimed at verifying every meaningful change to ts_store before it lands.

The automated matrix (`ts_test_cli`, tests 001–007 TS/XS, flags unit test, dual-compiler smoke) is the proof layer: millions of events, multiple persist modes, per-OS and per-disk result leaves, and promoted summaries under [test-summary/](test-summary/).

**C++23 modules** are used throughout tests and examples. They are **not for portability** — they speed up incremental compiles on a **single** machine + compiler combo. The goal is **not** to ship a reusable SDK for other repos. On one fixed toolchain, when you touch flags, a sink, or the core buffer, **only the affected module units recompile**. Tests and examples use `import`; implementation still lives under `include/beman/ts_store/ts_store_headers/` (module `.cppm` files are thin shims — moving bodies into modules is future work).

**New machine, CPU, OS, or compiler?** Rebuild **all** modules once from source (`./scripts/Build --FullRebuild=On`, or clean cmake — see [Building](#building) and [Doc/ARCHITECTURE.md § Modules are not for portability](Doc/ARCHITECTURE.md#modules-are-not-for-portability)). Only `modules/**/*.cppm` and companion `.cpp` sources are in git; BMIs (`.gcm`, `.pcm`, `.ddi`, etc.) are gitignored and must never be copied between environments. After that one full build on the new host, module artifacts can be **reused for testing and incremental dev on that same system** until the toolchain changes again.

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
- **Linux Mint 22.0 (OS_003 / ssd):** smoke matrix passes for **GCC 15** and **Clang 20** (113 scenarios each; 226 combined in manifest). Proof in [test-summary/OS_003/ssd/Smoke/](test-summary/OS_003/ssd/Smoke/).
- **RHEL rows** in [FileCheckList.txt](FileCheckList.txt) — on each target host, mark that host's rows `[x]` and run `./scripts/Build` (leave other rows `[ ]`).
- Heavy tests (005/006/007) scale up in `SIZE=full` (50 threads × 2k events; 005/007 × 3 runs) — tuned for ~30 min dual-compiler matrix on x7k. Only the last run performs persistence on 005/007 (see the test sources).
- Per-OS + per-compiler + per-disk separation: results live under `test-results/OS_00n/<compiler>/<disk>/Smoke|xFull/` and lightweight summaries are promoted to the equivalent path under `test-summary/`. GCC and Clang no longer share a leaf directory.
- `./scripts/Build` promotes summaries automatically after each checklist row; manual runs use [scripts/promote_summaries.sh](scripts/promote_summaries.sh).

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

Each leaf includes `run_manifest.jtext` (machine-readable matrix in jText **light profile**: `//` + `#` headers, `--` sections, `# Fields:` includes — see `Doc/ARCHITECTURE.md` and `jText/SPEC.md` §2.0), `README.md` (navigation), and `by_test/*.md` (per-binary detail).

Raw logs, .jtext/.bin artifacts, and per-run `.meta` files live under the (git-ignored) `test-results/` tree. Only the small promoted tree under `test-summary/` is intended to be committed.

---

## Dependencies

Tested on **RHEL 9** (AppStream). Other Linux distros need equivalent packages; compiler and Ninja version floors still apply.

### RHEL 9 / Fedora (`dnf`)

```bash
# Compilers, build tools, SQLite headers (full test matrix)
sudo dnf install -y \
  gcc-toolset-15* \
  clang \
  cmake \
  sqlite-devel

# Optional: system ninja-build (often 1.10.x on RHEL — too old for C++23 modules)
sudo dnf install -y ninja-build
```

### Linux Mint / Ubuntu 24.04+

Noble (Mint 22.x) ships **gcc-14** in official repos only. Full **C++23 module** builds need **GCC 15** — install via PPA, then point CMake at `g++-15`.

**→ Step-by-step:** [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md)

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-15 g++-15 libstdc++-15-dev cmake ninja-build sqlite3 libsqlite3-dev clang-20

cmake -G Ninja -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_C_COMPILER=gcc-15 \
      -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
      -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
      ..
```

Stock **g++-14** alone is not sufficient for the full module matrix on Mint (see [test-composer/build_report.txt](test-composer/build_report.txt)).

| Package | Purpose | Minimum |
|---------|---------|---------|
| `gcc-toolset-15*` | GCC for modules + matrix (`scl enable gcc-toolset-15`) | GCC **15** (smoke-tested: 15.2.1) |
| `clang-20` (Mint) / `clang` (RHEL) | Second compiler in matrix | Clang **18+** minimum; **21+** on RHEL for parity |
| `cmake` | Configure / generate | **3.26** |
| `sqlite-devel` | SQL persist + `ts_test_cli` summarize | libsqlite3 dev headers |
| **Ninja** | C++23 `FILE_SET cxx_modules` generator | **1.11+** (see below) |

**GCC usage:** RHEL’s default `/usr/bin/g++` is GCC 11 — not sufficient. Always build with the toolset:

```bash
scl enable gcc-toolset-15 -- bash          # interactive shell
scl enable gcc-toolset-15 -- cmake ...     # one-shot configure/build
```

**Ninja:** RHEL’s `ninja-build` package is often **1.10.x**, which CMake rejects for modules. `./scripts/Build` and `./scripts/build_dual_compilers.sh` both source [scripts/build_common.sh](scripts/build_common.sh) to auto-select Ninja ≥ 1.11 (e.g. `~/.local/bin/ninja`, CLion-bundled). Override manually if needed:

```bash
export NINJA=/path/to/ninja   # must be >= 1.11
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

**Repo dependencies (no extra `dnf`):** `vendor/jText` and `vendor/jacQlite` are committed — a plain clone builds in **vendored** mode. For live cross-project work, place `../jText` and `../jacQlite` next to this repo; the dual build script uses **reference** mode when siblings exist.

See [BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md](BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md) for common new-machine pitfalls.

---

## Building

### Recommended: checklist-driven (`./scripts/Build`)

Edit [FileCheckList.txt](FileCheckList.txt): **`[x]`** = run on this host, **`[ ]`** = skip. The script walks the file top to bottom, runs every `[x]` row, and **never modifies** the checklist.

```bash
# Smoke test for all [x] rows (e.g. RHEL 9.8 gcc + clang on this machine)
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off

# Full matrix (xFull leaf, ~30 min on x7k for heavy tests)
./scripts/Build FileCheckList.txt --FullRebuild=Off --SmokeTest=Off --FullTest=On
```

Per `[x]` row the script resolves Ninja (via [scripts/build_common.sh](scripts/build_common.sh)), picks the compiler (Mint GCC → `g++-15`, Mint Clang → `clang++-20`, RHEL GCC → `gcc-toolset-15`), builds the full stress-test matrix in transient `build-seq/`, runs `ts_test_cli`, promotes summaries, and removes the build tree.

Details: [FORWARDING.md](FORWARDING.md) · Mint toolchain: [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md)

### Manual cmake (ad-hoc / debugging)

C++23 modules require **Ninja ≥ 1.11** (see [Dependencies](#dependencies)).

**Compiler requirements:** **GCC 15+** for full module parity (gcc-toolset-15 on RHEL, **g++-15 via PPA on Mint 22**). **Clang 18+** (`clang++-20` on Mint; 21+ preferred on RHEL). GCC 14 on Mint is below smoke-tested parity for the module graph.

```bash
mkdir -p build-manual && cd build-manual
cmake -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-15 \
  -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
  -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
  -DCMAKE_BUILD_TYPE=Release \
  ..
cmake --build . --target ts_test_cli ts_store_flags -j"$(nproc)"
cd build-manual && ./ts_test_cli run --compiler gcc --disk ssd
```

**CMake options (Mint defaults):** `TS_STORE_GNU_RELEASE_O3=OFF` (avoids GCC 15 module ICE at `-O3`); `TS_STORE_CLANG_LTO=OFF` (avoids broken compile-only LTO links).

### Legacy dual-compiler script (RHEL bridge)

```bash
./scripts/build_dual_compilers.sh   # → build-dual/gcc, build-dual/clang
```

Still useful on RHEL for a one-shot gcc+clang pair. Primary workflow is `./scripts/Build`. See [DUAL_COMPILER_BUILD.md](DUAL_COMPILER_BUILD.md).

**Dependency modes** (`TS_STORE_JTEXT_MODE`, `TS_STORE_JACQLITE_MODE`):
- **`vendored`** (default) — `vendor/jText`, `vendor/jacQlite`
- **`reference`** — live siblings `../jText`, `../jacQlite` (auto-selected by `scripts/Build` when siblings exist)

[scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) copies siblings → `vendor/` (`--sync-all`).

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
- Progressive sizing (001–004 stay small; 005/006/007 reach 100k records/scenario in full mode)
- "Only the last run performs persistence" for 005/006/007 (earlier runs are pure hot-path measurement)
- OS auto-detection + `OS_00n` layout
- Promotion of lightweight summaries

See `get_test_params()` / `build_scenario_list()` in [modules/jac.test_framework/runner.cpp](modules/jac.test_framework/runner.cpp), plus the `main()` functions in the heavy test sources (e.g. `tests/ts_store_005/`, `tests/ts_store_007/`). Smoke matrix: **113 scenarios per compiler** (001–007 × TS/XS × 4 persist × 2 output modes + `ts_store_flags`); **226** after gcc+clang merge on the same leaf.

### Important current practices

- `DISK_TYPE` (x7k / 10k / ssd — normalized to exactly 3 characters) keeps results for different storage hardware completely separate.
- The runner **auto-detects** the OS (via `uname` + `/etc/os-release`) and assigns or re-uses a short padded `OS_001` / `OS_002` ... ID. This produces the nested layout `test-results/OS_001/<compiler>/<DISK_TYPE>/<SIZE_LABEL>/` (SIZE_LABEL = "Smoke" or "xFull"). 
  - A visible central list is kept in `test-results/OS_MAP.txt` (e.g. `OS_001 = RHEL 9.6`).
  - Full details for the run (including the assigned ID) are written to `OS_INFO.txt` inside the leaf directory.
  - You can force a specific ID with `OS_ID=OS_001` in params or `--os-id` on the CLI (override kept for debugging, weird containers/WSL/CI, migration, etc.).
  This design keeps all directory names short for perfect vertical alignment (padded OS_00n + 3-char disks x7k/10k/ssd + 5-char sizes) while automatically tracking which real OS each result set came from across machines. The promotion script mirrors the structure under `test-summary/`. (Padded form chosen so the framework can scale if it is successful across many OS variants.)
- `./scripts/Build` promotes after each checklist row. `./scripts/run_all_tests.sh` (legacy) runs gcc+clang from `build-dual/` and promotes. Direct `ts_test_cli run` does **not** promote — run `./scripts/promote_summaries.sh --all` yourself after a manual matrix run.

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

See [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md) for diagrams and the full structural map. Key directories:

- [modules/jac.ts_store/](modules/jac.ts_store/) — C++23 modules (`config`, `flags`, `ansi`, `core`, `impl.testing`, `test_options`, `persistence.{common,binary,jtext,sql,writer}`)
- [modules/jac.test_framework/](modules/jac.test_framework/) + [modules/jac.report/](modules/jac.report/) — matrix runner and summarization
- [include/beman/ts_store/ts_store_headers/](include/beman/ts_store/ts_store_headers/) — implementation headers (included by module units; prefer `import` in application code)
- [tests/ts_store_00N/](tests/) — the numbered stress test suites (001–007 TS/XS + flags unit test)
- [tests/test_params.txt](tests/test_params.txt) — controls `SIZE` (smoke/full), `DISK_TYPE`, selected tests, and `OS_ID`
- [scripts/Build](scripts/Build) + [FileCheckList.txt](FileCheckList.txt) — **primary** checklist-driven build + test + promote
- [scripts/ts-test](scripts/ts-test) — thin wrapper around `ts_test_cli` (searches common build dirs)
- [scripts/promote_summaries.sh](scripts/promote_summaries.sh) — `test-results/` → `test-summary/`
- [scripts/build_common.sh](scripts/build_common.sh) — Ninja ≥ 1.11 resolution
- Legacy: [scripts/build_dual_compilers.sh](scripts/build_dual_compilers.sh), [scripts/run_all_tests.sh](scripts/run_all_tests.sh)
- [scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) — siblings → `vendor/`
- [FORWARDING.md](FORWARDING.md) — sequential build design and current checklist status
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
- Smoke matrix on **Linux Mint 22.0 / ssd** is proven for **GCC 15** and **Clang 20**. RHEL rows still need runs on each target host (mark `[x]` on that host only).
- No automated CI yet — regression proof is manual smoke + promoted `test-summary/` commits.

---

## License

See LICENSE file.

**Related**: This work was developed in close collaboration with the [jText](https://github.com/JayACarlsonAtHome/jText) human-readable structured logging library.
