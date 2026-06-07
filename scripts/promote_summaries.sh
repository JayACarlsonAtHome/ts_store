#!/bin/bash
#
# promote_summaries.sh
#
# After a test run finishes, copy manifest + markdown navigation from the
# (git-ignored) test-results/ tree into test-summary/ for commit.
#
# Promoted per leaf (OS_00n/<disk>/<Smoke|xFull>/):
#   run_manifest.jtext, README.md, by_test/*.md
#
# Then regenerates test-summary/README.md hub index via ts_test_cli summarize-hub.
#
# Usage:
#   ./scripts/promote_summaries.sh                 # uses DISK_TYPE from tests/test_params.txt
#   ./scripts/promote_summaries.sh --disk 10k
#   ./scripts/promote_summaries.sh --all           # promote whatever exists under test-results/
#
# Called automatically at the end of run_all_tests.sh. Safe to run manually too.
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
    while IFS= read -r -d '' f; do
        leaf_dir=$(dirname "$f")
        rel=${leaf_dir#test-results/}
        LEAVES+=("$rel")
    done < <(find test-results \( -name run_manifest.jtext -o -name README.md \) -type f -print0 2>/dev/null | sort -z)
elif [[ -n "$DISK" ]]; then
    nd="$(normalize_disk "$DISK")"
    for cand in "$nd" "x7k" "10k" "ssd"; do
        for f in $(find "test-results" \( -path "*$cand*/README.md" -o -path "*$cand*/run_manifest.jtext" \) 2>/dev/null | head -10); do
            leaf_dir=$(dirname "$f")
            rel=${leaf_dir#test-results/}
            LEAVES+=("$rel")
        done
    done
else
    for cand in x7k 10k ssd; do
        for f in $(find "test-results" -path "*$cand*/README.md" 2>/dev/null | head -5); do
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
    echo "No manifests/READMEs found to promote under test-results/."
    exit 0
fi

echo "Promoting summaries for: ${LEAVES[*]}"

for rel in "${LEAVES[@]}"; do
    src="test-results/$rel"
    dst="test-summary/$rel"
    if [[ ! -f "$src/run_manifest.jtext" && ! -f "$src/README.md" ]]; then
        echo "  skip $rel (no manifest or README)"
        continue
    fi
    mkdir -p "$dst"
    cp -f "$src"/run_manifest.jtext "$dst/" 2>/dev/null || true
    cp -f "$src"/README.md "$dst/" 2>/dev/null || true
    if [[ -d "$src/by_test" ]]; then
        mkdir -p "$dst/by_test"
        cp -f "$src/by_test/"*.md "$dst/by_test/" 2>/dev/null || true
    fi
    count=$(find "$dst" -type f 2>/dev/null | wc -l | tr -d ' ')
    echo "  -> $dst/  (copied $count files)"
    find "$dst" -type f | sed 's/^/     /'
done

# Remove retired legacy summary files from promoted leaves
find test-summary -name 'TS_STORE_*_Summary.md' -type f -delete 2>/dev/null || true

# Regenerate hub index
HUB_CLI=""
for candidate in \
    "$PROJECT_ROOT/build-dual/gcc/ts_test_cli" \
    "$PROJECT_ROOT/build-dual/clang/ts_test_cli" \
    "$PROJECT_ROOT/build-matrix-smoke/ts_test_cli"; do
    if [[ -x "$candidate" ]]; then
        HUB_CLI="$candidate"
        break
    fi
done

if [[ -n "$HUB_CLI" ]]; then
    (cd "$PROJECT_ROOT" && "$HUB_CLI" summarize-hub) || echo "Warning: summarize-hub failed (rebuild ts_test_cli?)" >&2
else
    echo "Warning: ts_test_cli not found; skipped summarize-hub (run after ./scripts/build_dual_compilers.sh)" >&2
fi

echo "Done. test-summary/ contents are small and safe to git add/commit (test-results/ stays ignored)."