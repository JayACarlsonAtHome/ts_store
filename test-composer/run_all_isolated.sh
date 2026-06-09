#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUMMARY="$ROOT/test-composer/isolated_summary.txt"

TARGETS=(
  jtext_core
  jac_qlite
  jac_ts_store_config
  jac_ts_store_ansi
  jac_ts_store_flags
  jac_ts_store_test_options
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
  ts_store_binary_throughput_test
  ts_store_001_TS
)

echo "=== Isolated per-target builds $(date -Iseconds) ===" | tee "$SUMMARY"
PASS=0; FAIL=0
for t in "${TARGETS[@]}"; do
  echo "" | tee -a "$SUMMARY"
  echo "--- $t ---" | tee -a "$SUMMARY"
  if "$ROOT/test-composer/build_one.sh" "$t" 2>&1 | tail -5 | tee -a "$SUMMARY"; then
    if grep -q '^RESULT=PASS' "$ROOT/test-composer/isolated/${t}.result"; then
      ((PASS++)) || true
    else
      ((FAIL++)) || true
    fi
  else
    ((FAIL++)) || true
  fi
done
echo "" | tee -a "$SUMMARY"
echo "PASS=$PASS FAIL=$FAIL" | tee -a "$SUMMARY"