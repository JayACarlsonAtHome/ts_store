#!/bin/bash
#
# promote_summaries.sh
#
# After a test run finishes, copy the lightweight TS_STORE_*_Summary.md files
# out of the (git-ignored) test-results/ tree (which may be nested as
# test-results/OS_001/<DISK_TYPE>/<SIZE_LABEL>/ using the padded OS_00n convention)
# into the sibling test-summary/ tree (mirroring the nesting) so they can be committed and tracked.
#
# This keeps huge log/persist artifacts out of the repo while preserving the
# human-readable proof summaries per disk type (x7k/10k/ssd) and per OS/size.
#
# The central visible list lives in test-results/OS_MAP.txt (OS_001 = ..., OS_002 = ...).
# Full OS details for each run are in OS_INFO.txt inside the leaf directory.
#
# Usage:
#   ./scripts/promote_summaries.sh                 # uses DISK_TYPE from env or tests/test_params.txt
#   ./scripts/promote_summaries.sh --disk 10k
#   ./scripts/promote_summaries.sh --all           # promote whatever exists under test-results/
#
# Called automatically at the end of run_all_tests.sh (after the three summaries
# are written for the current DISK_TYPE). Safe to run manually too.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || { echo "ERROR: cannot cd to $PROJECT_ROOT"; exit 1; }

DISK=""
PROMOTE_ALL=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --disk)
            DISK="$2"; shift 2 ;;
        --all)
            PROMOTE_ALL=1; shift ;;
        -h|--help)
            echo "Usage: $0 [--disk x7k|10k|ssd] [--all]"; exit 0 ;;
        *)
            echo "Unknown arg: $1"; exit 1 ;;
    esac
done

# If no explicit disk, try to load the same way the runner does (from test_params.txt)
if [[ -z "$DISK" && $PROMOTE_ALL -eq 0 ]]; then
    CONFIG_FILE="$PROJECT_ROOT/tests/test_params.txt"
    if [[ -f "$CONFIG_FILE" ]]; then
        while IFS= read -r line || [[ -n "$line" ]]; do
            line="${line%%#*}"
            line="${line// /}"
            if [[ "$line" =~ ^DISK_TYPE=(.*)$ ]]; then
                DISK="${BASH_REMATCH[1]}"
                break
            fi
        done < "$CONFIG_FILE"
    fi
fi

# Normalize like the main runner (accepts many forms, outputs exactly 3-char for alignment)
normalize_disk() {
    local d="${1,,}"   # lowercase
    d="${d// /}"; d="${d//-/}"; d="${d//_/}"
    case "$d" in
        7k|7200|x7k|seven|seventyk) echo "x7k" ;;
        10k|10| x10k|tenk|ten)       echo "10k" ;;
        ssd|solid|flash)             echo "ssd" ;;
        *) echo "$d" ;;   # pass through if already good or unknown
    esac
}

declare -a LEAVES=()

if [[ $PROMOTE_ALL -eq 1 ]]; then
    # Discover *any* leaf directories (any depth) under test-results/ that contain a summary.
    # This supports both the old flat layout and the new nested layout:
    #   test-results/OS_001/<DISK>/<SIZE_LABEL>/   (using the padded OS_00n convention)
    # Full OS name lives in OS_INFO.txt inside the leaf; directory names stay short for alignment.
    while IFS= read -r -d '' f; do
        leaf_dir=$(dirname "$f")
        # Compute the relative path under test-results (works for 1 or 3 levels)
        rel=${leaf_dir#test-results/}
        LEAVES+=("$rel")
    done < <(find test-results -type f -name TS_STORE_Test_Summary.md -print0 2>/dev/null | sort -z)
elif [[ -n "$DISK" ]]; then
    nd="$(normalize_disk "$DISK")"
    # For explicit --disk we still support the simple case; user can pass full relative if needed
    for cand in "$nd" "x7k" "10k" "ssd"; do
        for f in $(find "test-results" -path "*$cand*/TS_STORE_Test_Summary.md" 2>/dev/null | head -5); do
            leaf_dir=$(dirname "$f")
            rel=${leaf_dir#test-results/}
            LEAVES+=("$rel")
        done
    done
else
    # Fallback: common simple disks (flat or under any OS)
    for cand in x7k 10k ssd; do
        for f in $(find "test-results" -path "*$cand*/TS_STORE_Test_Summary.md" 2>/dev/null | head -3); do
            leaf_dir=$(dirname "$f")
            rel=${leaf_dir#test-results/}
            LEAVES+=("$rel")
        done
    done
fi

# Dedup
if [[ ${#LEAVES[@]} -gt 0 ]]; then
    mapfile -t LEAVES < <(printf "%s\n" "${LEAVES[@]}" | sort -u)
fi

if [[ ${#LEAVES[@]} -eq 0 ]]; then
    echo "No summaries found to promote under test-results/ (any depth)."
    exit 0
fi

echo "Promoting summaries for: ${LEAVES[*]}"

for rel in "${LEAVES[@]}"; do
    src="test-results/$rel"
    dst="test-summary/$rel"
    if [[ ! -f "$src/TS_STORE_Test_Summary.md" ]]; then
        echo "  skip $rel (no TS_STORE_Test_Summary.md)"
        continue
    fi
    mkdir -p "$dst"
    cp -f "$src"/TS_STORE_*_Summary.md "$dst/" 2>/dev/null || true
    count=$(ls "$dst"/ 2>/dev/null | wc -l | tr -d ' ')
    echo "  -> $dst/  (copied $count files)"
    ls -1 "$dst/" | sed 's/^/     /'
done

echo "Done. test-summary/ contents are small and safe to git add/commit (test-results/ stays ignored)."
