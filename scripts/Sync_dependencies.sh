#!/bin/bash
#
# Sync_dependencies.sh
#
# Manages vendoring of external dependencies (primarily jText) for self-contained builds.
#
# Philosophy (per project conventions):
# - During development: use live sibling references (../jText) for small change surface.
# - Before push / for releases / self-contained deploys: vendor the exact tracked files locally.
# - A tracking list + sha256 checksums ensure "build exactly the same" and detect drift.
# - We (ts_store) want to be the vendor of our dependencies.
#
# Usage examples:
#   ./scripts/Sync_dependencies.sh --sync jText
#   ./scripts/Sync_dependencies.sh --verify jText
#   ./scripts/Sync_dependencies.sh --update-checksums jText
#
# After --sync, the tree under vendor/jText/ + vendor/sync/ should be committed for self-contained state.
# During heavy cross-project development, you can stay in reference mode (no need to sync every edit).
#
# The script is intentionally simple and auditable.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DEFAULT_SIBLING_BASE="../jText"
VENDOR_ROOT="$PROJECT_ROOT/vendor"
SYNC_DIR="$VENDOR_ROOT/sync"
JTEXT_VENDOR="$VENDOR_ROOT/jText"
JTEXT_CLI_VENDOR="$JTEXT_VENDOR/CLI"

TRACKED_FILE="$SYNC_DIR/jText.tracked"
CHECKSUMS_FILE="$SYNC_DIR/jText.sha256"

SIBLING_BASE="$DEFAULT_SIBLING_BASE"
ACTION=""
COMPONENT=""

usage() {
    echo "Sync_dependencies.sh - manage vendored dependencies (jText focus)"
    echo
    echo "Usage: $0 --sync jText | --verify jText | --update-checksums jText [--sibling /path/to/jText]"
    echo
    echo "  --sync jText              Copy tracked files from sibling into vendor/jText/ (and CLI subdir)"
    echo "  --verify jText            Verify that current sibling matches the pinned checksums"
    echo "  --update-checksums jText  Recompute checksums from the current sibling (after intentional update)"
    echo
    echo "Options:"
    echo "  --sibling PATH            Override sibling location (default: ../jText)"
    echo
    echo "After syncing, commit vendor/ changes for self-contained builds."
    echo "During active development across projects, you can build against live references without syncing."
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
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
        --sibling)
            SIBLING_BASE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            ;;
    esac
done

if [[ -z "$ACTION" || -z "$COMPONENT" ]]; then
    usage
fi

if [[ "$COMPONENT" != "jText" ]]; then
    echo "Currently only 'jText' component is supported."
    exit 1
fi

if [[ ! -f "$TRACKED_FILE" ]]; then
    echo "ERROR: Tracking file not found: $TRACKED_FILE"
    echo "Create it first (see vendor/sync/jText.tracked for example)."
    exit 1
fi

if [[ ! -d "$SIBLING_BASE" ]]; then
    echo "ERROR: Sibling directory not found: $SIBLING_BASE"
    echo "Use --sibling /path/to/jText if it is not at the default location."
    exit 1
fi

# Read tracked files (ignore comments and blank lines)
mapfile -t TRACKED < <(grep -v '^#' "$TRACKED_FILE" | grep -v '^[[:space:]]*$' || true)

if [[ ${#TRACKED[@]} -eq 0 ]]; then
    echo "No files listed in $TRACKED_FILE"
    exit 1
fi

compute_sums() {
    # Compute sha256 of the tracked files *as they exist in the sibling*
    # Output in the same format as sha256sum
    (cd "$SIBLING_BASE" && sha256sum "${TRACKED[@]}")
}

case "$ACTION" in
    update-checksums)
        echo "Updating checksums from sibling: $SIBLING_BASE"
        compute_sums > "$CHECKSUMS_FILE"
        echo "Wrote $CHECKSUMS_FILE"
        echo "Review and commit the updated checksums if the change is intentional."
        ;;

    verify)
        echo "Verifying against sibling: $SIBLING_BASE"
        if [[ ! -f "$CHECKSUMS_FILE" ]]; then
            echo "ERROR: No checksums file: $CHECKSUMS_FILE"
            echo "Run --update-checksums first."
            exit 1
        fi
        # Compute current sums and compare
        temp_sums=$(mktemp)
        compute_sums > "$temp_sums"
        if diff -u "$CHECKSUMS_FILE" "$temp_sums" > /dev/null; then
            echo "✓ All tracked files match the pinned checksums."
            rm -f "$temp_sums"
        else
            echo "✗ Checksum mismatch:"
            diff -u "$CHECKSUMS_FILE" "$temp_sums" || true
            rm -f "$temp_sums"
            exit 1
        fi
        ;;

    sync)
        echo "Syncing jText from sibling '$SIBLING_BASE' into '$JTEXT_VENDOR' (CLI tools to $JTEXT_CLI_VENDOR)"

        # Verify first (unless we want to allow out-of-spec sync? For safety, require match or have --force later)
        if [[ -f "$CHECKSUMS_FILE" ]]; then
            temp_sums=$(mktemp)
            compute_sums > "$temp_sums"
            if ! diff -q "$CHECKSUMS_FILE" "$temp_sums" > /dev/null; then
                echo "WARNING: Current sibling does not match pinned checksums."
                echo "If this is intentional, run --update-checksums first, then --sync again."
                echo "Or proceed at your own risk."
                # For now, continue (user can decide)
            fi
            rm -f "$temp_sums"
        fi

        mkdir -p "$JTEXT_VENDOR"
        mkdir -p "$JTEXT_CLI_VENDOR"

        for rel in "${TRACKED[@]}"; do
            src="$SIBLING_BASE/$rel"
            if [[ ! -f "$src" ]]; then
                echo "ERROR: Tracked file missing in sibling: $rel"
                exit 1
            fi

            if [[ "$rel" == src/tool/* ]]; then
                # Special mapping for CLI tools
                base=$(basename "$rel")
                dest="$JTEXT_CLI_VENDOR/$base"
            else
                dest="$JTEXT_VENDOR/$rel"
            fi

            mkdir -p "$(dirname "$dest")"
            cp -p "$src" "$dest"
            echo "  copied $rel -> $(realpath --relative-to="$PROJECT_ROOT" "$dest" 2>/dev/null || echo "$dest")"
        done

        echo "Sync complete."
        echo "Run --verify to confirm, or commit the changes under vendor/ for self-contained builds."
        ;;
esac

echo "Done."
