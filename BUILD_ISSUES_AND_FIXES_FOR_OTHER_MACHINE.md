# Build/Test Issues Encountered on This Machine (and Fixes)

> **2026-06 update:** Legacy `build_dual_compilers.sh`, `run_all_tests.sh`, and `test-composer/` are removed. Use `./scripts/Build` + [FileCheckList.txt](FileCheckList.txt) only.

This document explains the problems that occurred when trying to build and run the full test matrix for `ts_store` on the current machine (a different environment from the original development machine where the repo was created and originally built).

The goal is to help someone on the **original machine** (or any other clone) understand:
- Why things failed here
- What had to be fixed
- What the current recommended workflow is (especially for "local/reference" development with sibling `../jText` and `../jacQlite`)

---

## 1. Ninja Version Requirement (C++23 Modules)

### What went wrong
The first attempt to run the standard build (then `build_dual_compilers.sh`, now replaced by `./scripts/Build`):

```bash
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

failed immediately during CMake configure with errors like:

> The Ninja generator does not support C++20 modules using Ninja version 1.10.2 due to lack of required features. Ninja 1.11 or higher is required.

Many subsequent errors about modules not being supported by the generator.

### Root cause
- The project uses **C++23 modules** heavily (`CMAKE_CXX_SCAN_FOR_MODULES ON` in CMakeLists.txt, many `.cppm` files).
- C++ modules support in Ninja is relatively new and requires **Ninja >= 1.11**.
- This machine only had the system `ninja` (from `ninja-build-1.10.2-6.el9` package), which was too old.
- `./scripts/Build` sources [scripts/build_common.sh](scripts/build_common.sh), which prefers `command -v ninja` and can fall back to a CLion-bundled Ninja — but on this machine the system `ninja` (1.10.x) was found first and was too old.

### What we did
- Located the modern Ninja bundled with CLion:  
  `/home/jay/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64/ninja` (version 1.13.2).
- Overrode it when invoking the build script:

  ```bash
  NINJA=/home/jay/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64/ninja \
    ./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
  ```

- `build_common.sh` supports the `NINJA` environment variable for exactly this situation (see [README.md](README.md) and [FORWARDING.md](FORWARDING.md)).

### Recommendation for the original machine
- If you ever see this error, ensure your `ninja` (or `ninja-build`) is >= 1.11.
- On RHEL-like systems the packaged version is often old; use the one from CLion, a manual install, or `pip install ninja`.
- You can also pass `-DCMAKE_MAKE_PROGRAM=/path/to/good/ninja` directly to CMake.

---

## 2. jacQlite API Drift (SqlEventSink Compilation Failure)

### What went wrong
Even after fixing the Ninja version, the build (both the official dual build and the later local-mode build) failed while compiling:

```
include/beman/ts_store/ts_store_headers/persistence/SqlEventSink.cpp
```

With errors such as:

> ‘class jac::qlite::Sqlite::Statement’ has no member named ‘clear_bindings’
> ‘class jac::qlite::Sqlite::Statement’ has no member named ‘bind_int64’
> ... (same for bind_double)

This happened in the `insert_event()` method for the `_ints` and `_floats` (metric) tables. The main table used the modern variadic `bind(...)` API and compiled fine.

### Root cause
- `SqlEventSink` (used when `TS_STORE_ENABLE_SQLITE_PERSIST=ON`) was written against an older version of the jacQlite `Statement` class that exposed low-level indexed binding helpers.
- The current `../jacQlite` (and the vendored copy) only exposes the newer "peel first, recurse" variadic style:
  - `bind(Args&&...)`
  - `reset()`
  - `step()`
  - `get(...)` etc.
- No `clear_bindings()`, no `bind_int64(idx, val)`, no `bind_double(idx, val)`.
- Because the build was using **reference mode** (siblings), it picked up the newer header.

The heavy tests (005/006/007) and the SQL persistence module link against `jac_ts_store_persistence_sql`, which pulls this in.

### What we did
- Added the missing helper methods back to `Statement` in the sibling:

  ```cpp
  // In ../jacQlite/include/jacQLite/Sqlite.hpp (inside class Statement)
  void clear_bindings() { sqlite3_clear_bindings(stmt_); }
  void bind_int64(int idx, int64_t v) { sqlite3_bind_int64(stmt_, idx, v); }
  void bind_double(int idx, double v) { sqlite3_bind_double(stmt_, idx, v); }
  ```

- Applied the same patch to `vendor/jacQlite/include/jacQLite/Sqlite.hpp` for consistency (in case someone builds in vendored mode).
- Re-ran the build. It now succeeds for both GCC (via scl) and Clang.

This was a co-development drift between `ts_store` and `jacQlite`.

### Recommendation for the original machine
- The indexed helpers are now required for `SqlEventSink` to compile against current jacQlite.
- Keep them in the `Statement` class (or update `SqlEventSink` to only use the variadic `bind(...)` API by building argument lists at runtime).
- After editing the sibling, run `./scripts/Sync_dependencies.sh --sync-all` (or let the build script do it) so `vendor/` stays in sync.
- Consider whether the low-level indexed API should be part of jacQlite's public interface going forward (it is useful for dynamic column counts like the metric tables).

---

## 3. Local/Reference Mode vs Vendored Mode

### Background
- **Vendored** (default): Uses `vendor/jText` and `vendor/jacQlite` (self-contained clone).
- **Reference** ("local mode"): Uses live siblings at `../jText` and `../jacQlite`. This is what the dev machine uses.

`./scripts/Build` auto-detects siblings and adds:
```cmake
-DTS_STORE_JTEXT_MODE=reference
-DTS_STORE_JACQLITE_MODE=reference
```

It also runs `Sync_dependencies.sh --sync-all` at the end (and CMake attaches POST_BUILD sync hooks to many targets when both persist options are enabled).

### What we did here
- Early attempts used ad-hoc `build-dual/` and `build-local/` trees (removed workflow).
- The sequential checklist build (`build-seq/<platform>-<compiler>/`) now configures reference mode automatically when siblings exist.
- Each `[x]` row produces a transient build tree against the live siblings, runs the matrix, promotes, and deletes the tree.

**Caveat**: Even in reference mode, if `TS_STORE_ENABLE_JTEXT_PERSIST=ON` and `TS_STORE_ENABLE_SQLITE_PERSIST=ON` and the sibling directories exist on disk, CMake still injects the `Sync_dependencies.sh --sync-all` POST_BUILD steps. This is by design (see the bottom of `CMakeLists.txt`).

---

## 4. Running the Test CLI (`ts_test_cli`)

### What went wrong (initially)
When we tried to run smoke tests directly as:

```bash
./build-seq/Mint-GCC/ts_test_cli run --disk ssd
```

(from the project root, without `cd` into the build dir), we got hundreds of:

> ERROR: binary not found: "/home/jay/git/ts_store/ts_store_001_TS"

(and the same for every other test binary + all persist modes).

The run completed but reported total failure.

### Root cause
- The test binaries (`ts_store_001_TS`, `ts_store_005_XS`, `ts_store_flags`, etc.) are built alongside `ts_test_cli` inside the build directory.
- The runner (`modules/jac.test_framework/runner.cpp`) executes them by name, expecting them to be discoverable from the current working directory or next to the CLI binary.
- Invoking the CLI with a path from the project root left the CWD in the wrong place.

### What worked
```bash
cd build-seq/Mint-GCC   && ./ts_test_cli run --disk ssd
cd build-seq/Mint-Clang && ./ts_test_cli run --disk ssd
```

All 113 scenarios per compiler passed (smoke and later full/xFull).

### Recommendation
- Always `cd` into the build directory before running `ts_test_cli` (or use `./scripts/ts-test`, which finds `build-seq/*/ts_test_cli` and `cd`s there automatically).
- After `./scripts/Build`, the transient `build-seq/` tree may already be deleted — re-run Build or keep a manual cmake build dir if you need a long-lived tree.

---

## 5. Full vs Smoke Matrix + Promotion

- `tests/test_params.txt` controls `SIZE=smoke` vs `SIZE=full` (which produces `xFull` results).
- Full mode scales 005/006/007 to 50 threads × 2000 events × 3 runs (only the last run does persistence).
- After running full tests for both compilers, run:

  ```bash
  ./scripts/promote_summaries.sh --all
  ```

  to copy the lightweight summaries into the tracked `test-summary/OS_00n/<disk>/xFull/` tree.

We did exactly this for both smoke and full on this machine (with `DISK_TYPE=ssd` and a fake `OS_ID=OS_003`).

---

## Quick Checklist for the Original Machine

1. Make sure you have a good Ninja (>= 1.11). Export `NINJA=...` if the script picks the wrong one.
2. Keep the indexed bind helpers (`clear_bindings`, `bind_int64`, `bind_double`) in jacQlite's `Statement` (or modernize `SqlEventSink`).
3. For live development: keep `../jText` + `../jacQlite` next to the repo.
4. For a clean checklist build:

   ```bash
   NINJA=/path/to/good/ninja ./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
   ```

   Or manually (ad-hoc tree, not deleted automatically):

   ```bash
   mkdir -p build-manual && cd build-manual
   scl enable gcc-toolset-15 -- cmake -G Ninja -DCMAKE_MAKE_PROGRAM=... \
     -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
     -DTS_STORE_JTEXT_MODE=reference -DTS_STORE_JACQLITE_MODE=reference \
     ../..
   cmake --build . --target ts_test_cli ts_store_flags -j"$(nproc)"
   ```

5. Always run tests from *inside* the build directory (`./ts_test_cli run ...`).
6. Update `SIZE=full` in `tests/test_params.txt` when you want the heavy matrix.
7. Run promote after any significant matrix run.

---

## 6. Sequential Checklist Build (`./scripts/Build`)

### Current recommended workflow (2026-06-08+)

The primary path is [FileCheckList.txt](FileCheckList.txt) and:

```bash
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

Walks `FileCheckList.txt` top to bottom: **`[x]`** rows are built and tested; **`[ ]`** rows are skipped. Per `[x]` row: configure in transient `build-seq/<platform>-<compiler>/`, build full matrix, run `ts_test_cli`, promote, delete build tree. The checklist is **never auto-edited** — markers are user-selected only.

**Mint (OS_003 / ssd):** both rows pass smoke — GCC 15 (`g++-15` PPA) and Clang 20 (`clang++-20`).

**Fixes discovered during Mint Clang bring-up:**
- `TS_STORE_CLANG_LTO=OFF` (default) — compile-only `-flto=thin` produced LLVM bitcode `.o` files; linker expected ELF.
- `scripts/Build` resolves `clang++-20` when generic `clang++` is not on PATH.

See [FORWARDING.md](FORWARDING.md) and [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md).

---

## Current State on This Machine (for reference)

- **Checklist build:** `./scripts/Build` proven for Mint GCC + Clang smoke on ssd (`test-summary/OS_003/ssd/Smoke/`).
- Full smoke and full (`xFull`) matrices run successfully for gcc and clang on SSD.
- jacQlite sibling was patched for compatibility (`Statement` indexed bind helpers).
- **Reference** mode when `../jText` + `../jacQlite` exist; Ninja ≥ 1.11 via `scripts/build_common.sh`.

---

*Updated 2026-06-11 after legacy runner removal (`build_dual_compilers.sh`, `run_all_tests.sh`, `test-composer/`).*