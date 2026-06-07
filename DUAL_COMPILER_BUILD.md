//File:    DUAL_COMPILER_BUILD.md
//Date:    2026-06-07
//Purpose: Build documentation for ts_store dual-compiler workflow

# Dual Compiler Builds (GCC + Clang)

This project builds with **both** supported compilers when persistence and the test matrix are enabled (`TS_STORE_ENABLE_JTEXT_PERSIST=ON` and `TS_STORE_ENABLE_SQLITE_PERSIST=ON`).

**Supported compilers (required and smoke-tested):**

| Compiler | Minimum | Tested on this project |
|----------|---------|------------------------|
| **GCC** | **15** (`gcc-toolset-15` on RHEL 9) | GCC 15.2.1 |
| **Clang** | **21** (system `clang++`) | Clang 21.1.8 |

CMake enforces these floors when jText persist is ON. System GCC 11 (default on RHEL 9) is **not** sufficient — use `scl enable gcc-toolset-15`.

**Generator:** **Ninja** is required for C++23 `FILE_SET cxx_modules`. Plain Unix Makefiles will fail at module compile time.

**Siblings (reference mode, default):** `../jText` and `../jacQlite` must exist next to the repo for the canonical `ts_test_cli` build.

---

## Recommended Way to Build Both

```bash
./scripts/build_dual_compilers.sh
```

This script:

1. Clean-configures `build-dual/gcc` with gcc-toolset-15 and `build-dual/clang` with system `clang++` (if ≥ 21)
2. Uses **Ninja** (PATH, `$NINJA`, or CLion-bundled fallback)
3. Enables jText + SQLite persist
4. Builds these targets in **each** tree:

| Target | Role |
|--------|------|
| `ts_test_cli` | Matrix runner (required for smoke) |
| `ts_store_001_TS` … `ts_store_007_XS` | Stress tests |
| `ts_store_flags` | Flags unit test |
| `ts_store_jtext_high_throughput_test` | jText throughput example |
| `ts_store_jtext_split_demo` | jText split persistence demo |

**Not built by the script** (build manually if needed): `ts_store_binary_payload_benchmark`, `ts_store_double_buffer_demo`, `ts_store_integrated_double_buffer_demo`, and other standalone examples.

---

## Manual Dual Build

Use the same layout as the script: `build-dual/gcc` and `build-dual/clang`. Always pass `-G Ninja` and enable both persist options.

### With GCC (toolset 15)

```bash
# from repo root
scl enable gcc-toolset-15 -- bash -c '
  rm -rf build-dual/gcc && mkdir -p build-dual/gcc && cd build-dual/gcc
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
        -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
        ../..
  cmake --build . --target ts_test_cli ts_store_flags -j$(nproc)
'
```

### With Clang

```bash
# from repo root
rm -rf build-dual/clang && mkdir -p build-dual/clang && cd build-dual/clang
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
      -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
      ../..
cmake --build . --target ts_test_cli ts_store_flags -j$(nproc)
```

For the full stress-test set, add the same `--target` list as in `scripts/build_dual_compilers.sh`.

### Ninja not in PATH

```bash
export NINJA=/path/to/ninja   # honored by build_dual_compilers.sh
./scripts/build_dual_compilers.sh
```

Or pass `-DCMAKE_MAKE_PROGRAM=/path/to/ninja` on manual `cmake` invocations.

---

## Why Dual Compiler Support?

- Catches compiler-specific warnings and errors early (`-Werror` is on).
- Makes the code more portable.
- Clang tends to be stricter about certain diagnostics.
- Gives confidence that the jText improvements and ts_store persistence layer are solid.

CMake does not force a specific compiler — only basic version checks. Choose either toolchain per build directory.