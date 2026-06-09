//File:    DUAL_COMPILER_BUILD.md
//Date:    2026-06-07
//Purpose: Build documentation for ts_store dual-compiler workflow

# Dual Compiler Builds (GCC + Clang) — Legacy Bridge

> **Primary workflow:** [FORWARDING.md](FORWARDING.md) · `./scripts/Build FileCheckList.txt ...`  
> Use this dual script on RHEL when you want both compilers in one shot (`build-dual/gcc` + `build-dual/clang`). On Mint, use the checklist script (proven for GCC 15 + Clang 20 smoke on ssd).

This project builds with **both** supported compilers when persistence and the test matrix are enabled (`TS_STORE_ENABLE_JTEXT_PERSIST=ON` and `TS_STORE_ENABLE_SQLITE_PERSIST=ON`).

**Supported compilers (required and smoke-tested):**

| Compiler | Minimum | Notes |
|----------|---------|-------|
| **GCC** | **15** (gcc-toolset-15 on RHEL; **g++-15 PPA on Mint 22** — [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md)) | GCC 14 on Noble is not full-parity for modules |
| **Clang** | **18** (`clang++-20` on Mint; 21+ on RHEL) | 21+ preferred on RHEL |

The version floors are enforced (with helpful messages) only when jText persistence is enabled. On Linux Mint 22 / Ubuntu 24.04 the default GCC 13 is too old for C++23 module scanning — install g++-14 or clang-18.

**Generator:** **Ninja** is required for C++23 `FILE_SET cxx_modules`. Plain Unix Makefiles will fail at module compile time.

**Vendored mode (default):** `vendor/jText` + `vendor/jacQlite` — no siblings required (GitHub clone).

**Reference mode (dev):** when `../jText` and `../jacQlite` exist, `build_dual_compilers.sh` compiles from siblings and runs `--sync-all` afterward so `vendor/` stays push-ready.

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

### With GCC 15 (Mint / Ubuntu 24.04 — PPA)

See [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md) for PPA install. Summary:

```bash
# from repo root (after g++-15 installed)
rm -rf build-mint/gcc15 && mkdir -p build-mint/gcc15 && cd build-mint/gcc15
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_C_COMPILER=gcc-15 \
      -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
      -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
      ../..
cmake --build . --target ts_test_cli ts_store_flags -j$(nproc)
```

### With GCC (RHEL toolset 15)

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

### With Clang (Mint/Ubuntu or RHEL)

```bash
# from repo root
sudo apt install clang-18   # or clang on RHEL
rm -rf build-mint/clang && mkdir -p build-mint/clang && cd build-mint/clang
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++-18 \
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