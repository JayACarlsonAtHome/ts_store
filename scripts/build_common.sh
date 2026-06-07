#!/bin/bash
#
# build_common.sh — shared helpers for ts_store build scripts.
#
# Source from other scripts:
#   source "$(dirname "${BASH_SOURCE[0]}")/build_common.sh"
#

ts_store_ninja_minimum_major=1
ts_store_ninja_minimum_minor=11

# Return 0 when $1 is an executable ninja with version >= 1.11 (C++23 modules).
ts_store_ninja_version_ok() {
    local ninja_bin="$1"
    [[ -x "$ninja_bin" ]] || return 1

    local ver major minor
    ver="$("$ninja_bin" --version 2>/dev/null | head -1)"
    [[ -n "$ver" ]] || return 1

    major="${ver%%.*}"
    minor="${ver#*.}"
    minor="${minor%%.*}"

    if (( major > ts_store_ninja_minimum_major )); then
        return 0
    fi
    if (( major == ts_store_ninja_minimum_major && minor >= ts_store_ninja_minimum_minor )); then
        return 0
    fi
    return 1
}

# Print candidate ninja paths in preference order (deduplicated).
ts_store_ninja_candidates() {
    local -a seen=()
    local candidate

    _ts_store_emit_ninja_candidate() {
        local path="$1"
        [[ -n "$path" ]] || return 0
        for candidate in "${seen[@]}"; do
            [[ "$candidate" == "$path" ]] && return 0
        done
        seen+=("$path")
        printf '%s\n' "$path"
    }

    if command -v ninja >/dev/null 2>&1; then
        _ts_store_emit_ninja_candidate "$(command -v ninja)"
    fi

    _ts_store_emit_ninja_candidate "${HOME}/.local/bin/ninja"
    _ts_store_emit_ninja_candidate "${HOME}/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64/ninja"

    # Other CLion / Toolbox installs (versioned app dirs, if present).
    local toolbox_root="${HOME}/.local/share/JetBrains/Toolbox/apps"
    if [[ -d "$toolbox_root" ]]; then
        local app_dir ninja_path
        for app_dir in "$toolbox_root"/clion* "$toolbox_root"/CLion*; do
            [[ -d "$app_dir" ]] || continue
            ninja_path="$app_dir/bin/ninja/linux/x64/ninja"
            [[ -x "$ninja_path" ]] && _ts_store_emit_ninja_candidate "$ninja_path"
        done
    fi
}

# Set NINJA to a modules-capable binary. Honors $NINJA when version is sufficient.
# On failure, prints actionable guidance and returns non-zero.
ts_store_resolve_ninja() {
    local candidate ver

    if [[ -n "${NINJA:-}" ]]; then
        if ts_store_ninja_version_ok "$NINJA"; then
            ver="$("$NINJA" --version 2>/dev/null | head -1)"
            echo "Using ninja $ver (from \$NINJA): $NINJA"
            return 0
        fi
        ver="$("$NINJA" --version 2>/dev/null | head -1 || echo unknown)"
        echo "ERROR: \$NINJA=$NINJA reports version $ver; C++23 modules require ninja >= ${ts_store_ninja_minimum_major}.${ts_store_ninja_minimum_minor}." >&2
        return 1
    fi

    local rejected=""
    while IFS= read -r candidate; do
        [[ -n "$candidate" ]] || continue
        if ts_store_ninja_version_ok "$candidate"; then
            NINJA="$candidate"
            ver="$("$NINJA" --version 2>/dev/null | head -1)"
            echo "Using ninja $ver: $NINJA"
            return 0
        fi
        if [[ -x "$candidate" ]]; then
            ver="$("$candidate" --version 2>/dev/null | head -1 || echo unknown)"
            rejected="${rejected}  - $candidate ($ver)\n"
        fi
    done < <(ts_store_ninja_candidates)

    echo "ERROR: No suitable ninja found (need >= ${ts_store_ninja_minimum_major}.${ts_store_ninja_minimum_minor} for C++23 modules)." >&2
    if [[ -n "$rejected" ]]; then
        echo "Rejected (too old or unreadable):" >&2
        printf '%b' "$rejected" >&2
    fi
    echo "Install a newer ninja (pip install ninja, CLion-bundled binary, or manual download)," >&2
    echo "or export NINJA=/path/to/ninja before running the build script." >&2
    return 1
}