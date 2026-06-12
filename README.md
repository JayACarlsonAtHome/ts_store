//File:    README.md
//Date:    2026-06-06
//Purpose: Main project README for ts_store

# ts_store — Ultra-Fast, Thread-Safe Event Buffer

**High-throughput in-memory event collection with optional durable asynchronous persistence.**

The hot path (`save_event`) is designed to stay as close to pure in-memory speed as possible while a background thread drains to disk via pluggable sinks (Binary, jText split files, or SQLite).

**Structure & architecture:** [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md) — system layers, module graph, repo layout, test matrix, and build workflow.

---

## What this project is (and is becoming)

ts_store began as a **learning experiment**: C++23, bounded hot-path storage, flags, and persistence sinks explored incrementally. It is growing into a **regression platform** — not finished yet, but aimed at verifying every meaningful change to ts_store before it lands.

The automated matrix (`ts_test_cli`, tests 001–008 TS/XS, flags unit test, checklist-driven via [FileCheckList.txt](FileCheckList.txt)) is the proof layer: millions of events, multiple persist modes (binary, jText, SQL, in-memory, and flag-selective routing), per-OS and per-disk result leaves, and promoted summaries under [test-summary/](test-summary/).

**C++23 modules** are used throughout tests and examples. They are **not for portability** — they speed up incremental compiles on a **single** machine + compiler combo. The goal is **not** to ship a reusable SDK for other repos. On one fixed toolchain, when you touch flags, a sink, or the core buffer, **only the affected module units recompile**. Tests and examples use `import`; implementation still lives under `include/beman/ts_store/ts_store_headers/` (module `.cppm` files are thin shims — moving bodies into modules is future work).

**New machine, CPU, OS, or compiler?** Rebuild **all** modules once from source (`./scripts/Build FileCheckList.txt --FullRebuild=On`, or a clean cmake tree — see [Building](#building) and [Doc/ARCHITECTURE.md § Modules are not for portability](Doc/ARCHITECTURE.md#modules-are-not-for-portability)). Only `modules/**/*.cppm` and companion `.cpp` sources are in git; BMIs (`.gcm`, `.pcm`, `.ddi`, etc.) are gitignored and must never be copied between environments. After that one full build on the new host, module artifacts can be **reused for testing and incremental dev on that same system** until the toolchain changes again.

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

- **Extensively tested** — Stress tests **001–008** TS/XS plus `ts_store_flags` exercise the double-buffered persistence matrix (binary, jText, SQL, in-memory, and **flag-selective routing**) via **module imports**. See [tests/](tests/) and [scripts/Build](scripts/Build).
- **Linux Mint 22.0 (OS_003 / ssd):** **Smoke** — **113/113** per compiler (GCC 15 + Clang 20). **xFull** — **115/115** per compiler (adds test 008 flag-routing scenarios). Proof: [test-summary/README.md](test-summary/README.md), [test-summary/OS_003/gcc/ssd/](test-summary/OS_003/gcc/ssd/), [test-summary/OS_003/clang/ssd/](test-summary/OS_003/clang/ssd/) — each leaf includes hardware/hostname from `OS_INFO.txt`.
- **RHEL rows** in [FileCheckList.txt](FileCheckList.txt) — on each target host, mark that host's rows `[x]` and run `./scripts/Build` (leave other rows `[ ]`).
- **xFull scaling** (see [tests/test_params.txt](tests/test_params.txt) and `get_test_params()` in [runner.cpp](modules/jac.test_framework/runner.cpp)):
  - **005/007** — 50×2k × 3 runs (100k events/run; persist on final run only)
  - **006** — 50×2k single pass (100k events)
  - **008** — 50×20k × 3 runs (**1M events/run**; 10k `KeeperRecord` → jText + 10k `DatabaseEntry` → SQLite; persist verified on final run)
  - Tuned for ~30 min per compiler on **x7k**; on SSD the same matrix finishes in minutes.
- Results layout: `test-results/OS_00n/<compiler>/<disk>/Smoke|xFull/` → promoted to the same path under `test-summary/`. GCC and Clang are separate leaves.
- **Primary workflow:** `./scripts/Build` + [FileCheckList.txt](FileCheckList.txt). Legacy shell/Python matrix runners are removed. Manual `ts_test_cli` runs use [scripts/promote_summaries.sh](scripts/promote_summaries.sh).

The core in-memory path + `DoubleBufferedWriter` + pluggable sinks (JText, Binary, SQL, and `FlagRoutingEventSink`) is the primary delivered capability. Advanced query/aggregation beyond `select(id)` remains future work.

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

Test-specific runtime sizing (threads, events, runs, and the "progressive difficulty" + "only last run persists" rules for 005/007/008) is controlled by:

- [tests/test_params.txt](tests/test_params.txt)
- `get_test_params()` in [modules/jac.test_framework/runner.cpp](modules/jac.test_framework/runner.cpp) (invoked by `ts_test_cli`)
- Individual heavy test implementations (e.g. [tests/ts_store_005/test_005_TS.cpp](tests/ts_store_005/test_005_TS.cpp))

A 0-int / 0-float variant of the heaviest stress test (005) is also maintained for pure payload + flags pressure testing.

### Flags

A single `uint64_t` carries user hints (`KeeperRecord`, `DatabaseEntry`, `LogConsole`, `SendNetwork`, `IsResult`, severity, etc.) plus automatic `HasData` / `HasIntData` / `HasDblData` bits.

`KeeperRecord` and `DatabaseEntry` drive selective persistence: test **008** routes them through `FlagRoutingEventSink` to jText and SQLite respectively while the rest of the events stay in-memory on the persist path.

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
- `jac.ts_store.persistence.jtext` — `JTextEventSink`, split files (main + _Ints + _Floats); `PersistMode::KeeperOnly` filters to `KeeperRecord`
- `jac.ts_store.persistence.binary` — `BinaryEventSink`, fast length-prefixed mmap path
- `jac.ts_store.persistence.sql` — `SqlEventSink` (when SQLite persist is enabled at configure time); `PersistMode::DatabaseOnly` filters to `DatabaseEntry`
- `jac.ts_store.persistence.writer` — `DoubleBufferedWriter`
- `FlagRoutingEventSink` (header) — routes each batch to jText and/or SQL sinks by per-event flags

Sinks honor user flags: `KeeperRecord` → file path, `DatabaseEntry` → SQL. Test **008** proves this at 1M-event scale.

See [examples/](examples/) — all demos and benchmarks use `import`, not raw ts_store headers.

---

## Performance

The number that matters is sustained rate **while durably recording** in the background.

Typical results from the full stress matrix (see the latest summaries under `test-summary/`, generated by the runner):

- Hot path with async double-buffered jText/Binary: low millions of events/sec on realistic multi-threaded workloads.
- Pure in-memory (no persistence): significantly higher (tens of millions in some cases).

**Primary results documents** (committed, per-OS + per-disk):

The test framework uses an `OS_00n / compiler / disk / size` layout. Start at the hub:

- [test-summary/README.md](test-summary/README.md) — index of all promoted runs (`{OS}/{compiler}/{disk}/{Smoke|xFull}`)
- Example leaves: [test-summary/OS_003/gcc/ssd/Smoke/README.md](test-summary/OS_003/gcc/ssd/Smoke/README.md) (113 scenarios), [test-summary/OS_003/gcc/ssd/xFull/README.md](test-summary/OS_003/gcc/ssd/xFull/README.md) (115 scenarios) — per-test links under `by_test/`

Each leaf includes `run_manifest.jtext` (machine-readable matrix in jText **standard profile**: `//` + `#` headers, `--` sections, `# Fields:` includes — see [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md)), `OS_INFO.txt` (host snapshot), `README.md` (navigation), and `by_test/*.md` (per-binary detail).

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

Stock **g++-14** alone is not sufficient for the full module matrix on Mint (use **g++-15**).

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

**Ninja:** RHEL’s `ninja-build` package is often **1.10.x**, which CMake rejects for modules. `./scripts/Build` sources [scripts/build_common.sh](scripts/build_common.sh) to auto-select Ninja ≥ 1.11 (e.g. `~/.local/bin/ninja`, CLion-bundled). Override manually if needed:

```bash
export NINJA=/path/to/ninja   # must be >= 1.11
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

**Repo dependencies (no extra `dnf`):** `vendor/jText` and `vendor/jacQlite` are committed — a plain clone builds in **vendored** mode. For live cross-project work, place `../jText` and `../jacQlite` next to this repo; `./scripts/Build` uses **reference** mode when siblings exist.

See [BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md](BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md) for common new-machine pitfalls.

---

## Building

### Recommended: checklist-driven (`./scripts/Build`)

Edit [FileCheckList.txt](FileCheckList.txt): **`[x]`** = run on this host, **`[ ]`** = skip. The script walks the file top to bottom, runs every `[x]` row, and **never modifies** the checklist.

```bash
# Smoke test for all [x] rows (e.g. RHEL 9.8 gcc + clang on this machine)
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off

# Full matrix (xFull leaf; ~30 min per compiler on x7k, much faster on SSD)
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

**Dependency modes** (`TS_STORE_JTEXT_MODE`, `TS_STORE_JACQLITE_MODE`):
- **`vendored`** (default) — `vendor/jText`, `vendor/jacQlite`
- **`reference`** — live siblings `../jText`, `../jacQlite` (auto-selected by `scripts/Build` when siblings exist)

[scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) copies siblings → `vendor/` (`--sync-all`).

---

## Running the Automated Test Suite

**Primary path:** [scripts/Build](scripts/Build) + [FileCheckList.txt](FileCheckList.txt) (see [Building](#building)). That drives configure, build, `ts_test_cli run`, promote, and cleanup per `[x]` row.

**Manual / ad-hoc:** the matrix runner is [tools/test_cli/main.cpp](tools/test_cli/main.cpp) → `ts_test_cli`, with a thin finder at [scripts/ts-test](scripts/ts-test). Params: [tests/test_params.txt](tests/test_params.txt) (`SIZE`, `DISK_TYPE`, selected tests, optional `OS_ID`).

```bash
# After ./scripts/Build left a build-seq tree, or from build-manual/
./scripts/ts-test run --disk ssd

# Or from inside the build directory (required — binaries are cwd-adjacent)
cd build-manual && ./ts_test_cli run --compiler gcc --disk ssd
```

Legacy shell/Python runners are removed. One C++ matrix (`ts_test_cli` + `jac.test_framework`).

**Key behaviors** (runner + test sources):
- Progressive sizing — 001–004 stay small; 005/006/007 reach 100k events/run in xFull; **008 reaches 1M events/run**
- 005/007/008 — only the **last** run performs persistence; earlier runs measure hot path only
- Test **008** uses `persist=flags` only (2 scenarios/compiler: `008_TS` + `008_XS` in `flags_logs/`)

See `get_test_params()` / `build_scenario_list()` in [modules/jac.test_framework/runner.cpp](modules/jac.test_framework/runner.cpp), plus heavy test sources (e.g. [tests/ts_store_005/](tests/ts_store_005/), [tests/ts_store_007/](tests/ts_store_007/), [tests/ts_store_008/](tests/ts_store_008/)).

| Matrix | Scenarios / compiler | Notes |
|--------|---------------------|-------|
| **Smoke** | **113** | 001–007 × TS/XS × 4 persist × 2 output modes + `ts_store_flags` |
| **xFull** | **115** | Same + test 008 × TS/XS (`flags` persist) |

### Result layout and OS IDs

| Piece | Role |
|-------|------|
| `#OS_00n` in [FileCheckList.txt](FileCheckList.txt) | `./scripts/Build` sets `OS_ID` for each `[x]` row |
| `test-results/OS_00n/<compiler>/<disk>/Smoke\|xFull/` | Raw logs + manifest (gitignored) |
| `test-summary/…` (same path) | Promoted lightweight proof (committed) |
| `OS_INFO.txt` + manifest `HostInfo` | CPU, RAM, hostname captured at run start |

`DISK_TYPE` is normalized to exactly 3 characters (`x7k`, `10k`, `ssd`) so directory columns align. Override `OS_ID` in params or via CLI for manual runs outside Build.

Direct `ts_test_cli run` does **not** promote — run `./scripts/promote_summaries.sh --all` after a manual matrix.

```bash
# Raw logs (gitignored)
less -R test-results/OS_003/gcc/ssd/xFull/flags_logs/TS_STORE_TEST_008_TS/gcc_flags_off.log
less -R test-results/OS_003/gcc/ssd/Smoke/binary_logs/TS_STORE_TEST_005_TS/gcc_binary_on.log

# Promoted summary (committed)
less test-summary/OS_003/gcc/ssd/Smoke/README.md
```

Summaries live under `test-summary/OS_003/<compiler>/ssd/{Smoke,xFull}/` (hub: [test-summary/README.md](test-summary/README.md)). `test-results/` is gitignored; only `test-summary/` is committed.

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

Pure binary (no jText) examples and benchmarks are always available even without the jText option. The heavy stress tests (005/007 throughput, **008** flag-selective routing) are the best real-world usage references.

---

## Project Layout (key parts)

See [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md) for diagrams and the full structural map. Key directories:

- [modules/jac.ts_store/](modules/jac.ts_store/) — C++23 modules (`config`, `flags`, `ansi`, `core`, `impl.testing`, `test_options`, `persistence.{common,binary,jtext,sql,writer}`)
- [modules/jac.test_framework/](modules/jac.test_framework/) + [modules/jac.report/](modules/jac.report/) — matrix runner and summarization
- [include/beman/ts_store/ts_store_headers/](include/beman/ts_store/ts_store_headers/) — implementation headers (included by module units; prefer `import` in application code)
- [tests/ts_store_00N/](tests/) — numbered stress suites (001–008 TS/XS + `ts_store_flags` unit test)
- [tests/test_params.txt](tests/test_params.txt) — controls `SIZE` (smoke/full), `DISK_TYPE`, selected tests, and `OS_ID`
- [scripts/Build](scripts/Build) + [FileCheckList.txt](FileCheckList.txt) — **primary** checklist-driven build + test + promote
- [scripts/ts-test](scripts/ts-test) — thin wrapper around `ts_test_cli` (finds `build-seq/` trees)
- [scripts/promote_summaries.sh](scripts/promote_summaries.sh) — `test-results/` → `test-summary/`
- [scripts/build_common.sh](scripts/build_common.sh) — Ninja ≥ 1.11 resolution
- [scripts/Sync_dependencies.sh](scripts/Sync_dependencies.sh) — siblings → `vendor/`
- [FORWARDING.md](FORWARDING.md) — sequential build design and current checklist status
- [examples/](examples/) — demos, throughput benchmarks, and persistence examples
- [tools/jtext_cli/](tools/jtext_cli/) — jText CLI tools (process / retrieve)
- [tools/test_cli/](tools/test_cli/) — matrix CLI (`ts_test_cli`; invoked by `./scripts/Build`)
- [vendor/jText/](vendor/jText/) — vendored copy of the jText library
- [test-summary/](test-summary/) — committed lightweight summaries (`OS_00n/<compiler>/<disk>/Smoke|xFull/`)
- `test-results/` — raw logs, .jtext/.bin artifacts, .meta files (**git-ignored**, very large)

---

## Limitations & Future Work

- `SqlEventSink` exists and is in the stress matrix, but SQL persistence is optional at configure time and less battle-tested than jText/Binary on every OS leaf.
- No rotation, compaction, or retention policy on the persisted side.
- Query/aggregation beyond `select(id)` is not implemented at runtime.
- **Linux Mint 22.0 / ssd (OS_003):** Smoke **113/113** and xFull **115/115** proven for **GCC 15** and **Clang 20**. RHEL rows still need runs on each target host (mark `[x]` on that host only).
- No automated CI yet — regression proof is manual smoke + promoted `test-summary/` commits.

---

## License

See LICENSE file.

**Related**: This work was developed in close collaboration with the [jText](https://github.com/JayACarlsonAtHome/jText) human-readable structured logging library.
