# Linux Mint / Ubuntu 24.04 — Build Guide

Linux Mint 22.x is based on **Ubuntu Noble 24.04**. Official apt repos ship **gcc-14** as the newest GCC. They do **not** ship `g++-15`.

`ts_store` uses **C++23 modules** heavily. Full module parity requires **GCC 15+** (RHEL: `gcc-toolset-15`; Mint: PPA below). Stock **g++-14** fails on module consumers (ICE, bad BMI) even when individual libraries compile.

**Clang** on Mint (`clang++-20` or `clang++-18`) also passes the full smoke matrix. No generic `clang++` symlink — `scripts/Build` picks the newest versioned binary.

When **Linux Mint 23** ships (newer Ubuntu base with gcc-15 in main), the GCC PPA step should become optional.

---

## One-time: install toolchains

### GCC 15 (required for GCC checklist rows)

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-15 g++-15 libstdc++-15-dev
```

Verify:

```bash
g++-15 --version
# Expect: g++-15 (Ubuntu ...) 15.x
```

### Clang 20 (for Clang checklist rows)

```bash
sudo apt install clang-20
clang++-20 --version
# Expect: Ubuntu clang version 20.x
```

`clang-18` also works if 20 is unavailable.

### Build deps

```bash
sudo apt install cmake ninja-build sqlite3 libsqlite3-dev
```

**Ninja:** C++23 modules require **Ninja ≥ 1.11**. Mint’s `ninja-build` package is usually sufficient. If CMake complains:

```bash
pip install --user ninja
export NINJA=~/.local/bin/ninja
```

---

## Recommended: checklist-driven build

Both Mint rows are proven on **ssd** (smoke, 113 scenarios per compiler). On a Mint host, mark those rows `[x]` and leave other-platform rows `[ ]`:

```
[x] GCC    / Linux Mint 22.0 / ssd
[x] Clang  / Linux Mint 22.0 / ssd
```

Then run:

```bash
cd ~/git/ts_store
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

| Row type | Compiler picked by `scripts/Build` |
|----------|-------------------------------------|
| `GCC / Linux Mint …` | `g++-15`, `gcc-15` |
| `Clang / Linux Mint …` | `clang++-20` (falls back to -19, -18) |

The script walks the checklist top to bottom, runs each `[x]` row, builds under `build-seq/`, runs smoke tests, promotes to `test-summary/`, and deletes the transient build tree. It does **not** change `[x]` / `[ ]` markers.

Full matrix (`xFull`):

```bash
./scripts/Build FileCheckList.txt --FullRebuild=Off --SmokeTest=Off --FullTest=On
```

See [FORWARDING.md](../FORWARDING.md) for flag details and adding new rows (e.g. `10k` disk).

---

## Manual cmake (optional)

### GCC 15

```bash
cd ~/git/ts_store
rm -rf build-mint/gcc15 && mkdir -p build-mint/gcc15 && cd build-mint/gcc15

cmake -G Ninja .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 \
  -DCMAKE_C_COMPILER=gcc-15 \
  -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
  -DTS_STORE_ENABLE_SQLITE_PERSIST=ON

cmake --build . -j"$(nproc)"
./ts_test_cli run --compiler gcc --disk ssd
```

### Clang 20

```bash
rm -rf build-mint/clang20 && mkdir -p build-mint/clang20 && cd build-mint/clang20

cmake -G Ninja .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-20 \
  -DCMAKE_C_COMPILER=clang-20 \
  -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
  -DTS_STORE_ENABLE_SQLITE_PERSIST=ON

cmake --build . -j"$(nproc)"
./ts_test_cli run --compiler clang --disk ssd
```

Always run `ts_test_cli` from inside the build directory.

---

## CMake defaults on Mint

| Option | Default | Why |
|--------|---------|-----|
| `TS_STORE_GNU_RELEASE_O3` | `OFF` | GCC 15 PPA ICE at `-O3` on module consumers |
| `TS_STORE_NATIVE_TUNING` | `OFF` | `-march=native` can ICE GCC 15 + modules |
| `TS_STORE_CLANG_LTO` | `OFF` | Compile-only `-flto=thin` produced bitcode `.o` files that failed to link |

Use `-DCMAKE_BUILD_TYPE=Debug` if you prefer debug symbols without Release tuning.

---

## Compiler summary

| Compiler | Role on Mint |
|----------|----------------|
| **g++-15** (PPA) | Full module matrix (GCC rows) |
| **clang++-20** | Full module matrix (Clang rows) |
| **g++-14** (stock) | Unreliable for full `ts_store` modules |

Keep system `g++` unchanged. Pass explicit `-DCMAKE_CXX_COMPILER=...` to CMake.

---

## Related docs

- [README.md](../README.md) — project overview
- [ARCHITECTURE.md](ARCHITECTURE.md) — structure, modules, test matrix
- [FORWARDING.md](../FORWARDING.md) — sequential checklist workflow
- [BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md](../BUILD_ISSUES_AND_FIXES_FOR_OTHER_MACHINE.md) — Ninja, jacQlite drift, CLI cwd