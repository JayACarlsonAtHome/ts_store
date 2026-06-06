#!/bin/bash
#
# promote_summaries.sh
#
# After a test run finishes, copy the lightweight TS_STORE_*_Summary.md files
# out of the (git-ignored) test-results/<disk>/ tree into the sibling
# test-summary/<disk>/ tree so they can be committed and tracked.
#
# This keeps huge log/persist artifacts out of the repo while preserving the
# human-readable proof summaries per disk type (x7k/10k/ssd).
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

declare -a DISKS=()

if [[ $PROMOTE_ALL -eq 1 ]]; then
    # Discover any 3-char-looking subdirs under test-results/ that contain a summary
    for d in test-results/*/; do
        [[ -d "$d" ]] || continue
        base="$(basename "$d")"
        if [[ -f "$d/TS_STORE_Test_Summary.md" ]]; then
            DISKS+=("$base")
        fi
    done
elif [[ -n "$DISK" ]]; then
    nd="$(normalize_disk "$DISK")"
    DISKS+=("$nd")
else
    # Fallback: if a current TEST_RESULTS_BASE style dir exists with summaries, use its basename
    # or just try the common ones
    for cand in x7k 10k ssd; do
        if [[ -f "test-results/$cand/TS_STORE_Test_Summary.md" ]]; then
            DISKS+=("$cand")
        fi
    done
fi

if [[ ${#DISKS[@]} -eq 0 ]]; then
    echo "No disk summaries found to promote (test-results/<disk>/TS_STORE_*_Summary.md)."
    exit 0
fi

echo "Promoting summaries for disk(s): ${DISKS[*]}"

for d in "${DISKS[@]}"; do
    src="test-results/$d"
    dst="test-summary/$d"
    if [[ ! -f "$src/TS_STORE_Test_Summary.md" ]]; then
        echo "  skip $d (no TS_STORE_Test_Summary.md in $src)"
        continue
    fi
    mkdir -p "$dst"
    cp -f "$src"/TS_STORE_*_Summary.md "$dst/" 2>/dev/null || true
    echo "  -> $dst/  (copied $(ls "$dst"/ | wc -l | tr -d ' ') files)"
    ls -1 "$dst/" | sed 's/^/     /'
done

echo "Done. test-summary/ contents are small and safe to git add/commit (test-results/ stays ignored)."
