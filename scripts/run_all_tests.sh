#!/bin/bash
#
# run_all_tests.sh
#
# Automation to run all ts_store stress tests (001-007 TS/XS + flags).
# Supports selecting output (verbose or silent) and compiler.
# Logs all results.
# Can be used as the "automation" or "controls selector".
#
# Usage:
#   ./scripts/run_all_tests.sh --compiler gcc --output yes
#   ./scripts/run_all_tests.sh --compiler clang --output no
#   ./scripts/run_all_tests.sh   # defaults to gcc, output=yes (show on console + log)
#
# After run, results are in results/<compiler>/
#   - individual .log files
#   - summary.md
#
# The README links to results/gcc/summary.md and results/clang/summary.md
#
# Note: For GCC uses scl enable gcc-toolset-15.
# For full dual, run twice (once per compiler) or use the build_dual first.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

COMPILER="gcc"
OUTPUT_MODE="yes"  # yes = show on console (tee), no = silent to logs only

RESULTS_BASE="$PROJECT_ROOT/results"
LOG_DIR=""

usage() {
    echo "Usage: $0 [--compiler gcc|clang] [--output yes|no]"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --compiler)
            COMPILER="$2"
            shift 2
            ;;
        --output)
            OUTPUT_MODE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

if [[ "$COMPILER" != "gcc" && "$COMPILER" != "clang" ]]; then
    echo "Compiler must be gcc or clang"
    usage
fi

if [[ "$OUTPUT_MODE" != "yes" && "$OUTPUT_MODE" != "no" ]]; then
    echo "--output must be yes or no"
    usage
fi

echo "=== ts_store All Stress Tests Runner ==="
echo "Compiler: $COMPILER"
echo "Output mode: $OUTPUT_MODE (console visible: $OUTPUT_MODE)"
echo

# Determine build dir and how to invoke cmake/build
BUILD_DIR="$PROJECT_ROOT/build-results-$COMPILER"
CMAKE_CMD="cmake"
BUILD_CMD="cmake --build . --target"

if [[ "$COMPILER" == "gcc" ]]; then
    BUILD_PREFIX="scl enable gcc-toolset-15 -- bash -c"
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++"
    # We don't enable jtext here for the core stress tests; they don't need it.
    # The stress tests (001-007) are always built.
else
    BUILD_PREFIX=""
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++"
fi

echo "Using build dir: $BUILD_DIR"

# Only reconfigure/rebuild if the expected binaries are missing (to speed up repeated runs)
NEED_BUILD=0
for t in ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags; do
    if [[ ! -x "$BUILD_DIR/$t" ]]; then
        NEED_BUILD=1
        break
    fi
done

if [[ $NEED_BUILD -eq 1 ]]; then
    echo "Binaries missing or first run — cleaning and building..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    CONFIGURE_CMD="$CMAKE_CMD $CMAKE_ARGS '$PROJECT_ROOT'"

    if [[ "$COMPILER" == "gcc" ]]; then
        scl enable gcc-toolset-15 -- bash -c "$CONFIGURE_CMD"
    else
        eval "$CONFIGURE_CMD"
    fi

    echo "Building all stress test binaries..."
    TARGETS="ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags"

    if [[ "$COMPILER" == "gcc" ]]; then
        scl enable gcc-toolset-15 -- bash -c "cmake --build . --target $TARGETS -j\$(nproc)"
    else
        cmake --build . --target $TARGETS -j$(nproc)
    fi
    echo "Build complete."
else
    echo "All test binaries already present in $BUILD_DIR — skipping rebuild."
    cd "$BUILD_DIR"
fi

# Prepare results dir
mkdir -p "$RESULTS_BASE/$COMPILER"
LOG_DIR="$RESULTS_BASE/$COMPILER"
rm -f "$LOG_DIR"/*.log 2>/dev/null || true

# List of tests in order
declare -a TESTS=(
    "ts_store_001_TS"
    "ts_store_001_XS"
    "ts_store_002_TS"
    "ts_store_002_XS"
    "ts_store_003_TS"
    "ts_store_003_XS"
    "ts_store_004_TS"
    "ts_store_004_XS"
    "ts_store_005_TS"
    "ts_store_005_XS"
    "ts_store_006_TS"
    "ts_store_006_XS"
    "ts_store_007_TS"
    "ts_store_007_XS"
    "ts_store_flags"
)

echo
echo "=== Running all tests ==="
echo "Results will be logged to $LOG_DIR/"
echo

TOTAL_TESTS=${#TESTS[@]}
PASSED=0
FAILED=0

for test in "${TESTS[@]}"; do
    bin="./$test"
    if [[ ! -x "$bin" ]]; then
        echo "ERROR: binary $bin not found or not executable"
        ((FAILED++))
        continue
    fi

    log_file="$LOG_DIR/${test}.log"
    echo "Running $test ... (log: ${test}.log)"

    if [[ "$OUTPUT_MODE" == "yes" ]]; then
        # Show on console + log
        if ! "$bin" 2>&1 | tee "$log_file"; then
            echo "  -> $test FAILED (see log)"
            ((FAILED++))
        else
            echo "  -> $test PASSED"
            ((PASSED++))
        fi
    else
        # Silent: only to log, no console output from test
        if ! "$bin" > "$log_file" 2>&1; then
            echo "  -> $test FAILED (see log)"
            ((FAILED++))
        else
            echo "  -> $test PASSED"
            ((PASSED++))
        fi
    fi
done

echo
echo "=== All tests completed ==="
echo "Passed: $PASSED / $TOTAL_TESTS"
echo "Failed: $FAILED / $TOTAL_TESTS"
echo "Logs in: $LOG_DIR/"

# Generate summary markdown for this compiler
summary_file="$LOG_DIR/summary.md"
cat > "$summary_file" << EOF
# ts_store Stress Test Results — $COMPILER

**Run date**: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
**Compiler**: $COMPILER
**Output mode**: $OUTPUT_MODE
**Total tests**: $TOTAL_TESTS
**Passed**: $PASSED
**Failed**: $FAILED

## Summary Table

| Test | Status | Log |
|------|--------|-----|
EOF

for test in "${TESTS[@]}"; do
    log_file="${test}.log"
    if grep -q "PASS" "$LOG_DIR/$log_file" 2>/dev/null || grep -q "verified, zero corruption" "$LOG_DIR/$log_file" 2>/dev/null; then
        status="✅ PASS"
    else
        status="❌ FAIL"
    fi
    echo "| $test | $status | [log]($log_file) |" >> "$summary_file"
done

cat >> "$summary_file" << EOF

## Notes

- All tests use the internal verification harnesses (verify_level01 etc.).
- Tests 005 and 007 are the "big" 1M record massive tests (configurable; see source comments).
- Double-buffered persistence is active in the 005/007 runs (via recent updates attaching BinaryEventSink).
- Individual logs contain the full console output from each run.
- For dual-compiler comparison see the sibling Clang/GCC pages (linked from top-level README).

See the main [README.md](../../README.md) for overview.
EOF

echo "Summary written to $summary_file"
echo
echo "To view GCC results: results/gcc/summary.md"
echo "To view Clang results: results/clang/summary.md (run with --compiler clang)"
echo
echo "Done."