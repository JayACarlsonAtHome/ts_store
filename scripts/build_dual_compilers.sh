#!/bin/bash
#
# build_dual_compilers.sh
#
# Builds ts_store (with jText persistence enabled) using both supported compilers:
#   1. gcc-toolset-15 (GCC 15)
#   2. System clang (if available and recent enough)
#
# Usage:
#   ./scripts/build_dual_compilers.sh
#
# This script is intended for local development to verify that changes
# compile cleanly under both toolchains after modifications.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_BASE="$PROJECT_ROOT/build-dual"

echo "=== ts_store Dual Compiler Build ==="
echo "Project root: $PROJECT_ROOT"
echo

# Function to do a clean build with a specific compiler
build_with_compiler() {
    local compiler="$1"
    local build_dir="$2"
    local display_name="$3"

    echo "------------------------------------------------------------"
    echo "Building with: $display_name"
    echo "Build directory: $build_dir"
    echo "------------------------------------------------------------"

    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    cd "$build_dir"

    if [[ "$compiler" == gcc-toolset-15 ]]; then
        # Use devtoolset to get GCC 15
        scl enable gcc-toolset-15 -- bash -c "
            cmake -DCMAKE_BUILD_TYPE=Debug \
                  -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
                  -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
                  '$PROJECT_ROOT'
            cmake --build . --target ts_test_cli ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags ts_store_jtext_high_throughput_test ts_store_jtext_split_demo -j \$(nproc)
        "
    else
        # Assume it's a direct path to clang++ or just "clang++"
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_CXX_COMPILER="$compiler" \
              -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
              -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
              "$PROJECT_ROOT"

        cmake --build . --target ts_test_cli ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags ts_store_jtext_high_throughput_test ts_store_jtext_split_demo -j $(nproc)
    fi

    echo "✓ Build with $display_name completed successfully."
    echo
}

# 1. Build with gcc-toolset-15
build_with_compiler "gcc-toolset-15" "$BUILD_BASE/gcc" "gcc-toolset-15 (GCC 15)"

# 2. Build with clang (if available)
if command -v clang++ >/dev/null 2>&1; then
    CLANG_VERSION=$(clang++ --version | head -1)
    echo "Found clang++: $CLANG_VERSION"

    # Only try if it looks reasonably modern (clang 16+)
    MAJOR_VERSION=$(clang++ -dumpversion | cut -d. -f1)
    if [ "$MAJOR_VERSION" -ge 16 ]; then
        build_with_compiler "clang++" "$BUILD_BASE/clang" "clang++ $CLANG_VERSION"
    else
        echo "Skipping clang build: detected version $MAJOR_VERSION is too old (need 16+)."
    fi
else
    echo "clang++ not found in PATH. Skipping clang build."
fi

echo "================================================================"
echo "All requested dual-compiler builds completed."
echo "Build directories:"
echo "  - $BUILD_BASE/gcc"
[ -d "$BUILD_BASE/clang" ] && echo "  - $BUILD_BASE/clang"
echo "================================================================"