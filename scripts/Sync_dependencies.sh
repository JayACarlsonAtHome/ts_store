#!/bin/bash
#
# Sync_dependencies.sh
#
# Manages vendoring of external dependencies (jText, jacQlite) for self-contained builds.
#
# Philosophy:
# - During development (siblings present): compile from ../jText and ../jacQlite (reference mode).
# - After every successful build: sync sibling → vendor/ so git pushes stay self-contained.
# - Clones without siblings: build from vendor/ (vendored mode — CMake default).
#
# Usage:
#   ./scripts/Sync_dependencies.sh --sync-all              # update checksums + sync all (when siblings exist)
#   ./scripts/Sync_dependencies.sh --sync jText|jacQlite
#   ./scripts/Sync_dependencies.sh --verify jText|jacQlite|all
#   ./scripts/Sync_dependencies.sh --update-checksums jText|jacQlite|all
#
# Options:
#   --sibling-jtext PATH     Override ../jText
#   --sibling-jacqlite PATH  Override ../jacQlite
#   --require-sibling        Exit 1 if a requested component has no sibling (for POST_BUILD hooks)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VENDOR_ROOT="$PROJECT_ROOT/vendor"
SYNC_DIR="$VENDOR_ROOT/sync"

SIBLING_JTEXT="${SIBLING_JTEXT:-$PROJECT_ROOT/../jText}"
SIBLING_JACQLITE="${SIBLING_JACQLITE:-$PROJECT_ROOT/../jacQlite}"

ACTION=""
COMPONENT=""
REQUIRE_SIBLING=0

usage() {
    echo "Sync_dependencies.sh - vendor jText and jacQlite into ts_store"
    echo
    echo "Usage: $0 --sync-all | --sync jText|jacQlite | --verify jText|jacQlite|all | --update-checksums jText|jacQlite|all"
    echo
    echo "  --sync-all                Update checksums and sync every component whose sibling exists"
    echo "  --sync COMPONENT          Copy tracked files from sibling into vendor/"
    echo "  --verify COMPONENT        Verify sibling matches pinned checksums (COMPONENT may be 'all')"
    echo "  --update-checksums COMPONENT  Recompute checksums from sibling"
    echo
    echo "Options:"
    echo "  --sibling-jtext PATH      jText sibling (default: ../jText)"
    echo "  --sibling-jacqlite PATH   jacQlite sibling (default: ../jacQlite)"
    echo "  --require-sibling         Fail if sibling missing for the requested component"
    exit 1
}

tracked_file() {
    case "$1" in
        jText) echo "$SYNC_DIR/jText.tracked" ;;
        jacQlite) echo "$SYNC_DIR/jacQlite.tracked" ;;
        *) echo "Unknown component: $1" >&2; exit 1 ;;
    esac
}

checksums_file() {
    case "$1" in
        jText) echo "$SYNC_DIR/jText.sha256" ;;
        jacQlite) echo "$SYNC_DIR/jacQlite.sha256" ;;
        *) echo "Unknown component: $1" >&2; exit 1 ;;
    esac
}

vendor_root() {
    case "$1" in
        jText) echo "$VENDOR_ROOT/jText" ;;
        jacQlite) echo "$VENDOR_ROOT/jacQlite" ;;
        *) echo "Unknown component: $1" >&2; exit 1 ;;
    esac
}

sibling_root() {
    case "$1" in
        jText) echo "$SIBLING_JTEXT" ;;
        jacQlite) echo "$SIBLING_JACQLITE" ;;
        *) echo "Unknown component: $1" >&2; exit 1 ;;
    esac
}

read_tracked() {
    local tracked="$1"
    grep -v '^#' "$tracked" | grep -v '^[[:space:]]*$' || true
}

compute_sums() {
    local component="$1"
    local sibling
    sibling="$(sibling_root "$component")"
    local tracked
    tracked="$(tracked_file "$component")"
    mapfile -t files < <(read_tracked "$tracked")
    if [[ ${#files[@]} -eq 0 ]]; then
        echo "No files listed in $tracked" >&2
        exit 1
    fi
    (cd "$sibling" && sha256sum "${files[@]}")
}

sync_dest_for() {
    local component="$1"
    local rel="$2"
    local vendor
    vendor="$(vendor_root "$component")"

    if [[ "$component" == "jText" && "$rel" == src/tool/* ]]; then
        echo "$vendor/CLI/$(basename "$rel")"
    else
        echo "$vendor/$rel"
    fi
}

check_sibling() {
    local component="$1"
    local sibling
    sibling="$(sibling_root "$component")"
    if [[ ! -d "$sibling" ]]; then
        if [[ "$REQUIRE_SIBLING" -eq 1 ]]; then
            echo "ERROR: Sibling not found for $component: $sibling" >&2
            exit 1
        fi
        echo "Skipping $component: sibling not found at $sibling"
        return 1
    fi
    return 0
}

update_checksums_component() {
    local component="$1"
    check_sibling "$component" || return 0
    local out
    out="$(checksums_file "$component")"
    echo "Updating checksums for $component from $(sibling_root "$component")"
    compute_sums "$component" > "$out"
    echo "  wrote $out"
}

verify_component() {
    local component="$1"
    check_sibling "$component" || return 0
    local sums
    sums="$(checksums_file "$component")"
    if [[ ! -f "$sums" ]]; then
        echo "ERROR: No checksums file: $sums (run --update-checksums $component)" >&2
        exit 1
    fi
    local temp_sums
    temp_sums=$(mktemp)
    compute_sums "$component" > "$temp_sums"
    if diff -u "$sums" "$temp_sums" > /dev/null; then
        echo "✓ $component: sibling matches pinned checksums."
        rm -f "$temp_sums"
    else
        echo "✗ $component: checksum mismatch (sibling changed — run --sync-all or --update-checksums + --sync):" >&2
        diff -u "$sums" "$temp_sums" || true
        rm -f "$temp_sums"
        exit 1
    fi
}

sync_component() {
    local component="$1"
    check_sibling "$component" || return 0

    local sibling vendor tracked
    sibling="$(sibling_root "$component")"
    vendor="$(vendor_root "$component")"
    tracked="$(tracked_file "$component")"

    if [[ ! -f "$tracked" ]]; then
        echo "ERROR: Tracking file not found: $tracked" >&2
        exit 1
    fi

    echo "Syncing $component: $sibling → $vendor"
    mkdir -p "$vendor"

    mapfile -t files < <(read_tracked "$tracked")
    for rel in "${files[@]}"; do
        local src dest
        src="$sibling/$rel"
        if [[ ! -f "$src" ]]; then
            echo "ERROR: Tracked file missing in sibling: $src" >&2
            exit 1
        fi
        dest="$(sync_dest_for "$component" "$rel")"
        mkdir -p "$(dirname "$dest")"
        cp -p "$src" "$dest"
        echo "  copied $rel"
    done
    echo "  $component sync complete."
}

run_for_components() {
    local action_fn="$1"
    shift
    local components=("$@")
    for c in "${components[@]}"; do
        "$action_fn" "$c"
    done
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sync-all)
            ACTION="sync-all"
            shift
            ;;
        --sync)
            ACTION="sync"
            COMPONENT="${2:-}"
            shift 2
            ;;
        --verify)
            ACTION="verify"
            COMPONENT="${2:-}"
            shift 2
            ;;
        --update-checksums)
            ACTION="update-checksums"
            COMPONENT="${2:-}"
            shift 2
            ;;
        --sibling-jtext)
            SIBLING_JTEXT="$2"
            shift 2
            ;;
        --sibling-jacqlite)
            SIBLING_JACQLITE="$2"
            shift 2
            ;;
        --require-sibling)
            REQUIRE_SIBLING=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            ;;
    esac
done

if [[ -z "$ACTION" ]]; then
    usage
fi

ALL_COMPONENTS=(jText jacQlite)

case "$ACTION" in
    sync-all)
        for c in "${ALL_COMPONENTS[@]}"; do
            if check_sibling "$c"; then
                update_checksums_component "$c"
                sync_component "$c"
            fi
        done
        echo "Sync-all complete. Commit vendor/ + vendor/sync/*.sha256 before push."
        ;;
    sync)
        if [[ -z "$COMPONENT" ]]; then usage; fi
        if [[ "$COMPONENT" != "jText" && "$COMPONENT" != "jacQlite" ]]; then
            echo "Currently supported: jText, jacQlite" >&2
            exit 1
        fi
        sync_component "$COMPONENT"
        ;;
    verify)
        if [[ -z "$COMPONENT" ]]; then usage; fi
        if [[ "$COMPONENT" == "all" ]]; then
            run_for_components verify_component "${ALL_COMPONENTS[@]}"
        else
            verify_component "$COMPONENT"
        fi
        ;;
    update-checksums)
        if [[ -z "$COMPONENT" ]]; then usage; fi
        if [[ "$COMPONENT" == "all" ]]; then
            run_for_components update_checksums_component "${ALL_COMPONENTS[@]}"
        else
            update_checksums_component "$COMPONENT"
        fi
        ;;
esac

echo "Done."