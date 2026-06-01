# Dual Compiler Builds (GCC + Clang)

This project aims to build cleanly with **both** supported compilers when `TS_STORE_ENABLE_JTEXT_PERSIST=ON`:

- **gcc-toolset-15** (GCC 15)
- **Recent Clang** (Clang 16+ recommended, 21+ tested)

## Recommended Way to Build Both

After making changes, run:

```bash
./scripts/build_dual_compilers.sh
```

This script will:
1. Clean-build with gcc-toolset-15
2. Clean-build with system clang++ (if available and new enough)
3. Build the two main jText persistence test targets in both configurations

## Manual Dual Build

### With GCC 15
```bash
scl enable gcc-toolset-15 -- bash -c '
  rm -rf build-gcc && mkdir build-gcc && cd build-gcc
  cmake -DCMAKE_BUILD_TYPE=Debug -DTS_STORE_ENABLE_JTEXT_PERSIST=ON ..
  cmake --build . --target ts_store_jtext_high_throughput_test ts_store_jtext_split_demo -j$(nproc)
'
```

### With Clang
```bash
rm -rf build-clang && mkdir build-clang && cd build-clang
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
      ..
cmake --build . --target ts_store_jtext_high_throughput_test ts_store_jtext_split_demo -j$(nproc)
```

## Why Dual Compiler Support?

- Catches compiler-specific warnings and errors early.
- Makes the code more portable.
- `clang` tends to be stricter about certain things (unused private fields, dangling references, etc.).
- Gives confidence that the jText improvements and ts_store persistence layer are solid.

The CMake logic no longer forces a specific compiler — it only does basic version checks. You are free to choose either toolchain.