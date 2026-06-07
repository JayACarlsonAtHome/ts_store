#!/bin/bash
#
# run_all_tests.sh — thin wrapper (retired 2026-06)
#
# Formerly a ~1600-line shell matrix runner. Orchestration now lives in
# tools/test_cli (ts_test_cli). This script preserves the old entry point
# and flag names for existing habits/docs. (No CI workflow yet — run locally.)
#
# Usage:
#   ./scripts/run_all_tests.sh
#   ./scripts/run_all_tests.sh --compiler gcc --disk x7k
#   ./scripts/run_all_tests.sh --compiler all --disk ssd --os-id OS_003
#
# Requires: ./scripts/build_dual_compilers.sh (build-dual/{gcc,clang}/ts_test_cli)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

COMPILER="all"
DISK_TYPE=""
OS_ID=""
OUTPUT_MODE=""
PARAMS_FILE="$PROJECT_ROOT/tests/test_params.txt"
TEMP_PARAMS=""

usage() {
    cat <<'EOF'
Usage: ./scripts/run_all_tests.sh [options]

Options:
  --compiler gcc|clang|all   Default: all (gcc then clang from build-dual/)
  --disk x7k|10k|ssd         Storage label (3-char). Falls back to tests/test_params.txt
  --os-id OS_003             Override OS_ID in test params (optional)
  --output yes|no            Legacy flag (accepted; CLI always runs on+off scenarios)
  -h, --help                 Show this help

Results: test-results/<OS_ID>/<disk>/<Smoke|xFull>/...
Runner:   build-dual/{gcc,clang}/ts_test_cli run ...

Build first:
  ./scripts/build_dual_compilers.sh
EOF
}

cleanup() {
    [[ -n "$TEMP_PARAMS" && -f "$TEMP_PARAMS" ]] && rm -f "$TEMP_PARAMS"
}
trap cleanup EXIT

normalize_disk() {
    case "${1,,}" in
        7k|7200|x7k|x7200)   echo x7k ;;
        10k|10000|x10k|x10000) echo 10k ;;
        ssd|solidstate|xssd) echo ssd ;;
        x7k|10k|ssd)         echo "$1" ;;
        *)
            echo "Warning: unknown DISK_TYPE '$1' — using as-is (expected x7k, 10k, or ssd)" >&2
            echo "$1"
            ;;
    esac
}

load_disk_from_params() {
    if [[ ! -f "$PARAMS_FILE" ]]; then
        return 1
    fi
    local line key val
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"
        line="${line// /}"
        [[ -z "$line" || "$line" != *"="* ]] && continue
        key="${line%%=*}"
        val="${line#*=}"
        if [[ "$key" == "DISK_TYPE" && -n "$val" ]]; then
            DISK_TYPE="$val"
            return 0
        fi
    done < "$PARAMS_FILE"
    return 1
}

write_params_override() {
    TEMP_PARAMS="$(mktemp "${TMPDIR:-/tmp}/ts_test_params.XXXXXX")"
    local line key
    while IFS= read -r line || [[ -n "$line" ]]; do
        local stripped="${line%%#*}"
        stripped="${stripped// /}"
        if [[ "$stripped" == OS_ID=* || "$stripped" == DISK_TYPE=* ]]; then
            continue
        fi
        printf '%s\n' "$line"
    done < "$PARAMS_FILE" > "$TEMP_PARAMS"
    if [[ -n "$DISK_TYPE" ]]; then
        echo "DISK_TYPE=$DISK_TYPE" >> "$TEMP_PARAMS"
    fi
    if [[ -n "$OS_ID" ]]; then
        echo "OS_ID=$OS_ID" >> "$TEMP_PARAMS"
    fi
    PARAMS_FILE="$TEMP_PARAMS"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compiler)
            COMPILER="${2:-}"
            shift 2
            ;;
        --disk)
            DISK_TYPE="${2:-}"
            shift 2
            ;;
        --os-id)
            OS_ID="${2:-}"
            shift 2
            ;;
        --output)
            OUTPUT_MODE="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -n "$OUTPUT_MODE" && "$OUTPUT_MODE" != "yes" && "$OUTPUT_MODE" != "no" ]]; then
    echo "ERROR: --output must be yes or no" >&2
    usage
    exit 1
fi

if [[ -z "$DISK_TYPE" ]]; then
    load_disk_from_params || true
fi
if [[ -z "$DISK_TYPE" ]]; then
    echo "ERROR: DISK_TYPE not set (use --disk x7k|10k|ssd or DISK_TYPE=... in tests/test_params.txt)" >&2
    usage
    exit 1
fi
DISK_TYPE="$(normalize_disk "$DISK_TYPE")"

if [[ -n "$OS_ID" ]]; then
    write_params_override
fi

if [[ "$COMPILER" != "gcc" && "$COMPILER" != "clang" && "$COMPILER" != "all" ]]; then
    echo "ERROR: --compiler must be gcc, clang, or all" >&2
    usage
    exit 1
fi

run_compiler() {
    local comp="$1"
    local build_dir="$PROJECT_ROOT/build-dual/$comp"
    local cli="$build_dir/ts_test_cli"

    if [[ ! -x "$cli" ]]; then
        echo "ERROR: $cli not found or not executable." >&2
        echo "Run: ./scripts/build_dual_compilers.sh" >&2
        return 1
    fi

    echo
    echo "=== run_all_tests.sh → ts_test_cli ($comp, disk=$DISK_TYPE) ==="
    local -a args=(run --compiler "$comp" --disk "$DISK_TYPE" --params "$PARAMS_FILE")
    (cd "$build_dir" && "$cli" "${args[@]}")
}

FAILED=0
if [[ "$COMPILER" == "all" ]]; then
    run_compiler gcc || FAILED=1
    run_compiler clang || FAILED=1
else
    run_compiler "$COMPILER" || FAILED=1
fi

if [[ "$FAILED" -ne 0 ]]; then
    echo
    echo "=== run_all_tests.sh: FAILED ===" >&2
    exit 1
fi

echo
echo "=== run_all_tests.sh: all compiler runs passed ==="

# Legacy behavior: promote lightweight summaries when they exist.
"$SCRIPT_DIR/promote_summaries.sh" --all 2>/dev/null || true

exit 0