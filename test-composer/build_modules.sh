#!/usr/bin/env bash
# Build ts_store module targets one at a time and record pass/fail.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/test-composer/build"
LOG="$ROOT/test-composer/build_results.txt"
COMPILER="${COMPILER:-g++-14}"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== ts_store module build probe ===" | tee "$LOG"
echo "Date: $(date -Iseconds)" | tee -a "$LOG"
echo "Compiler: $COMPILER ($($COMPILER --version | head -1))" | tee -a "$LOG"
echo "Build dir: $BUILD_DIR" | tee -a "$LOG"
echo "" | tee -a "$LOG"

cmake "$ROOT" \
  -G Ninja \
  -DCMAKE_CXX_COMPILER="$COMPILER" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON \
  -DTS_STORE_ENABLE_JTEXT_PERSIST=ON \
  -DTS_STORE_ENABLE_SQLITE_PERSIST=ON \
  -DTS_STORE_JTEXT_MODE=vendored \
  -DTS_STORE_JACQLITE_MODE=vendored \
  -DTS_STORE_BUILD_TEST_MATRIX=ON \
  2>&1 | tee -a "$LOG"

# Order: vendor libs first, then ts_store modules in dependency order.
TARGETS=(
  jtext_core
  jacQLite
  jac_ts_store_config
  jac_ts_store_ansi
  jac_ts_store_flags
  jac_ts_store_test_options
  jac_qlite
  jac_ts_store_persistence_common
  jac_ts_store_persistence_binary
  jac_ts_store_persistence_writer
  jac_jtext
  jac_ts_store_persistence_jtext
  jac_ts_store_persistence_sql
  jac_ts_store_core
  jac_ts_store_impl_testing
  jac_report
  jac_test_framework
  ts_test_cli
  ts_store_flags
  ts_store_001_TS
  ts_store_001_XS
)

echo "" | tee -a "$LOG"
echo "=== Per-target builds ===" | tee -a "$LOG"

PASS=0
FAIL=0
SKIP=0

for target in "${TARGETS[@]}"; do
  echo "" | tee -a "$LOG"
  echo "--- TARGET: $target ---" | tee -a "$LOG"
  TARGET_LOG="$ROOT/test-composer/logs/${target}.log"
  mkdir -p "$(dirname "$TARGET_LOG")"

  if ! ninja -t targets all 2>/dev/null | grep -q "^${target}:"; then
    echo "SKIP: target not in build graph" | tee -a "$LOG"
    ((SKIP++)) || true
    continue
  fi

  if ninja "$target" >"$TARGET_LOG" 2>&1; then
    echo "PASS: $target" | tee -a "$LOG"
    ((PASS++)) || true
  else
    echo "FAIL: $target (see $TARGET_LOG)" | tee -a "$LOG"
    tail -30 "$TARGET_LOG" | tee -a "$LOG"
    ((FAIL++)) || true
  fi
done

echo "" | tee -a "$LOG"
echo "=== Summary ===" | tee -a "$LOG"
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP" | tee -a "$LOG"