#!/usr/bin/env bash
# Build a single CMake target in a fresh isolated build dir.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:?usage: build_one.sh <target>}"
BUILD_DIR="$ROOT/test-composer/isolated/${TARGET}"
COMPILER="${COMPILER:-g++-14}"
LOG="$ROOT/test-composer/isolated/${TARGET}.result"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

{
  echo "TARGET=$TARGET"
  echo "COMPILER=$($COMPILER --version | head -1)"
  echo "START=$(date -Iseconds)"

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
    >/dev/null

  if ninja "$TARGET"; then
    echo "RESULT=PASS"
  else
    echo "RESULT=FAIL"
    exit 1
  fi
  echo "END=$(date -Iseconds)"
} >"$LOG" 2>&1 || true

cat "$LOG"
grep -E '^(RESULT=|TARGET=)' "$LOG"