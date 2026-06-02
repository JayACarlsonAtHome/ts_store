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
#   ./scripts/run_all_tests.sh --compiler clang --output yes
#   ./scripts/run_all_tests.sh   # defaults to gcc, output=yes (show on console + log)
# Use --output no if you only want logs (no live test output on console).
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

# Strip ANSI color escape sequences. This makes the saved .log files clean plain text
# (good for Markdown, GitHub, Vim, editors, git grep, etc.) while still allowing
# pretty colors when a human runs a test binary directly in a real terminal.
strip_ansi() {
    sed -r 's/\x1B\[[0-9;]*[mGK]//g'
}

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

if [[ "$COMPILER" == "gcc" ]]; then
    COMPILER_DISPLAY="GCC 15 (gcc-toolset-15)"
else
    COMPILER_DISPLAY="Clang 21.1 (Red Hat)"
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
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON"
    # Enable jText for the big double-buffer tests (005/007) so they can use
    # JTextSplitEventLog via JTextEventSink, producing separate _Ints.jtext and
    # _Floats.jtext files with all the metric data (in addition to the main log).
    # The stress tests (001-007) are always built.
else
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON"
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
        log_file="$LOG_DIR/${test}.log"
        echo "ERROR: binary $bin not found or not executable" > "$log_file"
        echo "=== TEST FAILED BY RUNNER ===" >> "$log_file"
        FAILED=$((FAILED + 1))
        continue
    fi

    log_file="$LOG_DIR/${test}.log"
    echo "Running $test ... (log: ${test}.log)"

    if [[ "$OUTPUT_MODE" == "yes" ]]; then
        # Show on console + log
        # CLI flags --interactive=0 --color=0 force non-interactive / no-color for automated runs.
        # The parse_test_options() in test mains sets the corresponding env vars so the
        # is_interactive()/colors_enabled() helpers (and Config defaults) pick them up.
        # This lets the programs be controlled from command line.
        # We strip ANSI for the .log files so they are clean in Markdown/Vim/etc.
        "$bin" --interactive=0 --color=0 < /dev/null 2>&1 | tee >(strip_ansi > "$log_file")
        bin_status=${PIPESTATUS[0]}
        if [ $bin_status -ne 0 ]; then
            echo "  -> $test FAILED (see log)"
            echo "=== TEST FAILED BY RUNNER ===" >> "$log_file"
            FAILED=$((FAILED + 1))
        else
            echo "  -> $test PASSED"
            echo "=== TEST PASSED BY RUNNER ===" >> "$log_file"
            PASSED=$((PASSED + 1))
        fi
    else
        # Silent: only to log, no console output from test
        # Always strip for the saved log file.
        "$bin" --interactive=0 --color=0 < /dev/null 2>&1 | strip_ansi > "$log_file"
        bin_status=${PIPESTATUS[0]}
        if [ $bin_status -ne 0 ]; then
            echo "  -> $test FAILED (see log)"
            echo "=== TEST FAILED BY RUNNER ===" >> "$log_file"
            FAILED=$((FAILED + 1))
        else
            echo "  -> $test PASSED"
            echo "=== TEST PASSED BY RUNNER ===" >> "$log_file"
            PASSED=$((PASSED + 1))
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
**Compiler**: $COMPILER_DISPLAY
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
    if grep -q "=== TEST PASSED BY RUNNER ===" "$LOG_DIR/$log_file" 2>/dev/null; then
        status="✅ PASS"
    elif grep -q "=== TEST FAILED BY RUNNER ===" "$LOG_DIR/$log_file" 2>/dev/null; then
        status="❌ FAIL"
    elif grep -qE 'PASS|passed verification!|All .* tests PASSED|ALL TESTS COMPLETED SUCCESSFULLY|verified, zero corruption|STRUCTURALLY PERFECT' "$LOG_DIR/$log_file" 2>/dev/null; then
        status="✅ PASS"
    else
        status="❌ FAIL"
    fi
    echo "| $test | $status | [log]($log_file) |" >> "$summary_file"
done

cat >> "$summary_file" << EOF

## Notes

- All tests use the internal verification harnesses (verify_level01 etc.).
- Tests 005 and 007 are the large-scale "massive" tests (historically ~1,000,000 records per run × 50 runs; the THREADS/EVENTS_PER_THREAD limits are configurable — see source comments in the test files).
- Double-buffered persistence (using JTextEventSink / JTextSplitEventLog + DoubleBufferedWriter) is enabled for the 005/007 runs. This produces separate main .jtext + _Ints.jtext + _Floats.jtext files containing ALL the int and float metric values (separate from the runner's stdout capture log). The hot path stays fast; background thread drains.
- All other tests exercise core features (flags handling, different scales, timestamped vs non-timestamped variants).
- Every test that reached the verification stage passed with 100% structural integrity (zero corruption reported) when the runner reported PASSED.
- Individual logs contain the full console output from each test binary (some are large due to debug-style dumps in lower-numbered tests).
- Raw logs and this summary are in this directory. All tests were driven by the automation in \`scripts/run_all_tests.sh\` (supports --compiler and --output yes/no for console vs logs-only selection).

See the main [README.md](../../README.md) for overview.
EOF

echo "Summary written to $summary_file"
echo
if [[ "$COMPILER" == "gcc" ]]; then
    echo "To view GCC results: results/gcc/summary.md"
    echo "To view Clang results: results/clang/summary.md (run with --compiler clang)"
else
    echo "To view Clang results: results/clang/summary.md"
    echo "To view GCC results: results/gcc/summary.md (run with --compiler gcc)"
fi
echo
echo "Done."