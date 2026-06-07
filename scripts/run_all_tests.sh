#!/bin/bash
#
# run_all_tests.sh
#
# Automation to run all ts_store stress tests (001-007 TS/XS + flags).
# Now exercises double-buffered persistence (async) for *every* test, once with
# BinaryEventSink and once with JTextEventSink (via --persist).
#
# Output logs + persist artifacts go under:
#   test-results/x7k/binary_logs/TS_STORE_TEST_001_TS/
#   test-results/x7k/jText_logs/TS_STORE_TEST_001_TS/
#   (and same for all other tests; flags gets logs in both for structure)
#
# Combinational fields (the dimensions whose product determines the scenario count):
#   1. Compiler      : gcc, clang   (2 when --compiler all / default)
#   2. Test          : the 15 values in the TESTS array below
#   3. LogType       : binary, jtext
#   4. OutputMode    : on (live + ANSI colors), off (silent)   [within each test+logtype, sorted on before off, then clang before gcc]
#
# Per-compiler count : 15 × 2 × 2 = 60
# Full "all" count   : 2 × 60 = 120 scenarios
#
# Each scenario produces one .log + one .meta (plus the persist artifacts written by the test).
#
# A rich summary document is (re)generated at the project root
# with OS, compiler column, per-compiler times (build+test), total suite duration, record counts, duration, persist size/rate,
# which log type was faster/smaller per (test, logtype, output mode), links, Config settings, etc.
#
# Usage:
#   ./scripts/run_all_tests.sh                  # default: runs all (gcc + clang internally)
#   ./scripts/run_all_tests.sh --compiler gcc --output yes
#   ./scripts/run_all_tests.sh --compiler clang --output yes
#   ./scripts/run_all_tests.sh --compiler all   # explicit all (both)
#
# After run (default does both compilers internally and one combined summary), see the summary and
# test-results/<DISK_TYPE>/ tree. README.md links to the summary.
#
# Cleanup is automatic ("roll out the garbage"): stray/old artifacts, previous per-compiler
# logs/metas/persists, inmem, and stray persist files in root/build dirs are removed at start
# so you don't have to manually clean before re-running. test-results/ is .gitignore'd (only
# the root summaries are committed).
#
# Note: GCC path uses "scl enable gcc-toolset-15".
# Always passes --interactive=0.
# For output=on (live): --color=1 so ANSI colors are visible on the console.
# For output=off (silent): --color=0. Saved .log files are always stripped of ANSI.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Ensure we are conceptually in the project root for the run (robust to being invoked
# as ./scripts/run_all_tests.sh from the scripts/ dir, from root, or with full path).
cd "$PROJECT_ROOT" || { echo "ERROR: cannot change to project root $PROJECT_ROOT"; exit 1; }

# =============================================================================
# Load test parameters from tests/test_params.txt (if present).
# This lets us [x] select tests and choose smoke (100 records, SSD friendly) vs full (high intensity).
# Most runs use SIZE=smoke; occasional full for real stress.
# DISK_TYPE controls storage-specific results dir: test-results/x7k , test-results/10k , test-results/ssd (all exactly 3 chars so they line up vertically)
# =============================================================================
CONFIG_FILE="$PROJECT_ROOT/tests/test_params.txt"
SIZE="smoke"
THREADS=5
EVENTS_PER_THREAD=20
RUNS=1
WRITER_THREADS=5
OPS_PER_THREAD=20
DISK_TYPE=""
OS_ID=""
declare -A SELECTED_TESTS=()
if [[ -f "$CONFIG_FILE" ]]; then
    echo "Loading test params from $CONFIG_FILE"
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"   # strip comments
        line="${line// /}"   # remove spaces
        [[ -z "$line" ]] && continue
        if [[ "$line" =~ ^SIZE=(.*)$ ]]; then
            SIZE="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^THREADS=(.*)$ ]]; then
            THREADS="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^EVENTS_PER_THREAD=(.*)$ ]]; then
            EVENTS_PER_THREAD="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^RUNS=(.*)$ ]]; then
            RUNS="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^WRITER_THREADS=(.*)$ ]]; then
            WRITER_THREADS="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^OPS_PER_THREAD=(.*)$ ]]; then
            OPS_PER_THREAD="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^DISK_TYPE=(.*)$ ]]; then
            DISK_TYPE="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^OS_ID=(.*)$ ]]; then
            OS_ID="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^([0-9]{3})=[xX] ]]; then
            SELECTED_TESTS["${BASH_REMATCH[1]}"]=1
        fi
    done < "$CONFIG_FILE"
fi

# Apply SIZE profile (smoke wins unless explicit high in config or SIZE=full)
if [[ "$SIZE" == "smoke" ]]; then
    THREADS=5; EVENTS_PER_THREAD=20; RUNS=1; WRITER_THREADS=5; OPS_PER_THREAD=20
elif [[ "$SIZE" == "full" ]]; then
    THREADS=250; EVENTS_PER_THREAD=4000; RUNS=50; WRITER_THREADS=50; OPS_PER_THREAD=400
fi

# SIZE_LABEL chosen for 5-char vertical alignment under the 3-char disk dirs
if [[ "$SIZE" == "full" ]]; then
    SIZE_LABEL="xFull"
else
    SIZE_LABEL="Smoke"
fi

# test-results/ layout: supports OS_ID (OS_001/OS_002... padded style) + disk (3-char) + size (5-char)
# for cross-OS / cross-machine / cross-disk testing while preserving vertical alignment.
# Full OS details go into OS_INFO.txt inside the leaf dir.
# A central visible list is kept in test-results/OS_MAP.txt (OS_00n = real OS name)
if [[ -n "$OS_ID" ]]; then
    TEST_RESULTS_BASE="$PROJECT_ROOT/test-results/$OS_ID/$DISK_TYPE/$SIZE_LABEL"
else
    TEST_RESULTS_BASE="$PROJECT_ROOT/test-results/$DISK_TYPE"
fi
BINARY_LOGS="$TEST_RESULTS_BASE/binary_logs"
JTEXT_LOGS="$TEST_RESULTS_BASE/jText_logs"
SQL_LOGS="$TEST_RESULTS_BASE/sql_logs"
INMEM_LOGS="$TEST_RESULTS_BASE/inmem_logs"
INMEM_DIR="$TEST_RESULTS_BASE/inmem"
mkdir -p "$BINARY_LOGS"
mkdir -p "$JTEXT_LOGS"
mkdir -p "$SQL_LOGS"
mkdir -p "$INMEM_LOGS"
mkdir -p "$INMEM_DIR"

# Build list of tests to run (from config selection or all)
declare -a TESTS=()
for t in 001 002 003 004 005 006 007; do
    if [[ ${#SELECTED_TESTS[@]} -eq 0 || -n "${SELECTED_TESTS[$t]:-}" ]]; then
        TESTS+=("ts_store_${t}_TS" "ts_store_${t}_XS")
    fi
done
if [[ ${#SELECTED_TESTS[@]} -eq 0 ]]; then
    TESTS+=("ts_store_flags")
fi

echo "Test selection: ${#TESTS[@]} binaries (SIZE=$SIZE threads=$THREADS events=$EVENTS_PER_THREAD runs=$RUNS)"

# Tools for jText → SQL roundtrip verification (jtext_process for .sql emit, jacQLite CLIs for load/export).
# Used in the post-test phase after jtext persist runs (points 3/4/6/7 in matrix *).
# This is jText-mediated SQL work via CLIs, not native direct-to-SQL INSERTs written by the ts_store exe itself.
JTEXT_PROCESS_BIN="/home/jay/git/jText/cmake-build-debug/jtext_process"
JAC_J2S_BIN="/home/jay/git/jacQLite/build/tools/jtext_integration/jtext_to_sqlite"
JAC_S2J_BIN="/home/jay/git/jacQLite/build/tools/jtext_integration/sqlite_to_jtext"
# Fallback search if moved
for cand in "$JTEXT_PROCESS_BIN" /home/jay/git/jText/build*/jtext_process /home/jay/git/jText/cmake-build-*/jtext_process; do [[ -x "$cand" ]] && JTEXT_PROCESS_BIN="$cand" && break; done
for cand in "$JAC_J2S_BIN" /home/jay/git/jacQLite/build*/tools/jtext_integration/jtext_to_sqlite; do [[ -x "$cand" ]] && JAC_J2S_BIN="$cand" && break; done
for cand in "$JAC_S2J_BIN" /home/jay/git/jacQLite/build*/tools/jtext_integration/sqlite_to_jtext; do [[ -x "$cand" ]] && JAC_S2J_BIN="$cand" && break; done

# === Cleanup of legacy/unused stuff (part of the runner now) ===
# Remove old results/ dir (legacy)
if [ -d "$PROJECT_ROOT/results" ]; then
  echo "Removing legacy results/ directory..."
  rm -rf "$PROJECT_ROOT/results"
fi

# Legacy flat test_results/ (before per-disk test-results/<DISK_TYPE>/ )
if [ -d "$PROJECT_ROOT/test_results" ]; then
  echo "Removing legacy flat test_results/ directory (now use test-results/<DISK_TYPE>/ )..."
  rm -rf "$PROJECT_ROOT/test_results"
fi

# Remove historical unused build directories that clutter the tree
for d in build-clean-check build-double-test build-dual build-no-persist; do
  if [ -d "$PROJECT_ROOT/$d" ]; then
    echo "Removing unused build directory: $d"
    rm -rf "$PROJECT_ROOT/$d"
  fi
done

# Roll out the garbage: aggressively clean stray/old artifacts from test-results/<DISK_TYPE>/
# and other places (persist files left in root/build dirs from manual runs, old
# non-prefixed logs, etc.). This is part of the runner so you don't have to
# manually "rm -rf test-results/*" before every full run. Keeps the structure
# for the current run's data. Summaries are re-generated at the end.
echo "Rolling out the garbage (stray artifacts from previous runs)..."
for logdir in "$BINARY_LOGS" "$JTEXT_LOGS"; do
  for sub in "$logdir"/TS_STORE_TEST_* ; do
    [ -d "$sub" ] || continue
    # delete anything that isn't a current gcc_/clang_ log/meta or a persist* file
    # (old runs or manual junk without compiler prefix)
    find "$sub" -maxdepth 1 -type f ! -name 'gcc_*' ! -name 'clang_*' ! -name 'persist*' -delete 2>/dev/null || true
  done
done
# stray inmem files without proper prefix
find "$INMEM_DIR" -maxdepth 1 -type f ! -name 'gcc_*' ! -name 'clang_*' -delete 2>/dev/null || true

# stray persist artifacts that can accumulate in root (from examples or manual runs)
rm -f "$PROJECT_ROOT"/{persist,persist_Ints,persist_Floats}.{bin,jtext,sql} 2>/dev/null || true

# also clean any that ended up in the build-results dirs (sometimes from running inside build)
for bdir in build-results-gcc build-results-clang; do
  if [ -d "$PROJECT_ROOT/$bdir" ]; then
    rm -f "$PROJECT_ROOT/$bdir"/{persist,persist_Ints,persist_Floats}.{bin,jtext,sql} 2>/dev/null || true
  fi
done

# Strip ANSI color escape sequences. This makes the saved .log files clean plain text
# (good for Markdown, GitHub, Vim, editors, git grep, etc.) while still allowing
# pretty colors when a human runs a test binary directly in a real terminal.
strip_ansi() {
    sed -r 's/\x1B\[[0-9;]*[mGK]//g'
}

# Map e.g. "ts_store_005_TS" -> "TS_STORE_TEST_005_TS"
# "ts_store_flags" -> "TS_STORE_TEST_flags"
test_to_subdir_name() {
    local t="$1"
    if [[ "$t" == "ts_store_flags" ]]; then
        echo "TS_STORE_TEST_flags"
    else
        local base="${t#ts_store_}"
        echo "TS_STORE_TEST_${base}"
    fi
}

# Extract a rough "number of records" from a captured test log (best effort).
# For massive tests this is TOTAL*RUNS.
extract_record_count() {
    local logf="$1"
    # Try several patterns the tests emit
    local n
    n=$(grep -oE 'FINAL MASSIVE TEST — [0-9,]+ entries| [0-9,]+ entries × [0-9]+ runs|All [0-9,]+ (entries|ENTRIES)|with [0-9,]+ entries' "$logf" 2>/dev/null | head -1 | grep -oE '[0-9,]+' | tr -d ',' | head -1 || true)
    if [[ -z "$n" ]]; then
        n=$(grep -oE 'TOTAL[^0-9]*[0-9,]+|expected: [0-9,]+' "$logf" 2>/dev/null | head -1 | grep -oE '[0-9,]+' | tr -d ',' | head -1 || true)
    fi
    echo "${n:-?}"
}

# Format seconds as "Xm Ys (Zs)" or just "Zs" for small values.
# Accepts number or "skipped".
format_duration() {
    local secs="$1"
    if [[ "$secs" == "skipped" || "$secs" == "N/A" ]]; then
        echo "$secs"
        return
    fi
    if [[ ! "$secs" =~ ^[0-9]+$ ]]; then
        echo "$secs"
        return
    fi
    local min=$(( secs / 60 ))
    local rem=$(( secs % 60 ))
    if (( min > 0 )); then
        printf "%2dm %2ds (%3ds)" "$min" "$rem" "$secs"
    else
        printf "%ds" "$secs"
    fi
}

# Emit a timestamp-prefixed entry to a summary file.
# This puts the datetime at the *beginning* of "log entry" style lines
# (the per-result bullets in the InMemory, SQL, and faster sections of summaries)
# so each recorded observation carries when it was noted.
summary_entry() {
    local f="$1"; shift
    local ts
    ts=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
    echo "$ts $*" >> "$f"
}

# Helper to prepare the log dir for a given test + persist type + output mode.
# Sets globals: logdir_name, sdir, ldir
prepare_log_dir() {
    local tname="$1"
    local pval="$2"
    if [[ "$pval" == "jtext" ]]; then
        logdir_name="jText_logs"
    elif [[ "$pval" == "sql" ]]; then
        logdir_name="sql_logs"
    elif [[ "$pval" == "none" ]]; then
        logdir_name="inmem_logs"
    else
        logdir_name="binary_logs"
    fi
    sdir=$(test_to_subdir_name "$tname")
    ldir="$TEST_RESULTS_BASE/$logdir_name/$sdir"
    mkdir -p "$ldir"
}

# Generate the rich markdown summary document.
# Scans under the computed TEST_RESULTS_BASE (which may be nested as
# test-results/OS_001/<DISK_TYPE>/<SIZE_LABEL>/ etc. using the OS_00n convention).
# Includes OS info, compile time, per-compiler times (build + test suite), compiler column, faster log per scenario, total suite duration, etc.
generate_test_summary() {
    local out_file="$1"
    local compiler="$2"
    local compiler_display="$3"
    local build_info="$4"
    local results_base="$5"
    local passed="$6"
    local failed="$7"
    local total_scenarios="$8"
    local suite_duration_sec="${9:-?}"
    local per_compiler_times="${10:-}"

    local suite_dur_display=$(format_duration "$suite_duration_sec")

    local now=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
    local os_pretty="unknown"
    if [[ -r /etc/os-release ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release 2>/dev/null || true
        os_pretty="${PRETTY_NAME:-$NAME $VERSION}"
    else
        os_pretty=$(uname -sr)
    fi

    # Header
    cat > "$out_file" <<HDR
# TS_STORE Test Results Summary

**Date**: $now  
**OS**: $os_pretty  
**Disk/Storage**: $DISK_TYPE  
**Summary generated by**: scripts/run_all_tests.sh (double-buffered persist on all tests)

## Build / Compile Info
**Compiler(s)**: $compiler_display  
**Build**: $build_info  
**Per-compiler times**:
$per_compiler_times
**Total suite duration**: $suite_dur_display

HDR

    echo "DEBUG: after HDR write, out_file=$out_file, results_base=$results_base" >> /tmp/summary_debug.log

    # Collect runs by scanning metas
    # We'll build rows and also compute per-(test,compiler) faster logtype
    declare -a rows=()
    declare -A scenario_data=()   # key=SUBDIR|COMPILER|OUTPUT_MODE  value="b_dur b_size b_rec j_dur j_size j_rec"
    declare -A seen=()            # dedup rows by full key (in case old metas linger)

    # Known tests and approx records for completeness when metas are missing
    declare -A KNOWN_TESTS=(
        ["TS_STORE_TEST_001_TS"]=64
        ["TS_STORE_TEST_001_XS"]=64
        ["TS_STORE_TEST_002_TS"]=100
        ["TS_STORE_TEST_002_XS"]=100
        ["TS_STORE_TEST_003_TS"]=100
        ["TS_STORE_TEST_003_XS"]=100
        ["TS_STORE_TEST_004_TS"]=100
        ["TS_STORE_TEST_004_XS"]=100
        ["TS_STORE_TEST_005_TS"]=100
        ["TS_STORE_TEST_005_XS"]=100
        ["TS_STORE_TEST_006_TS"]=100
        ["TS_STORE_TEST_006_XS"]=100
        ["TS_STORE_TEST_007_TS"]=100
        ["TS_STORE_TEST_007_XS"]=100
        ["TS_STORE_TEST_flags"]="?"
    )
    declare -A KNOWN_PERSIST=( ["binary_logs"]="binary" ["jText_logs"]="jtext" ["sql_logs"]="sql" ["inmem_logs"]="none" )

    for logtype_dir in "$results_base/binary_logs" "$results_base/jText_logs" "$results_base/sql_logs" "$results_base/inmem_logs"; do
        [[ -d "$logtype_dir" ]] || continue
        for sub in "$logtype_dir"/TS_STORE_TEST_* ; do
            [[ -d "$sub" ]] || continue
            local subname=$(basename "$sub")
            for meta in "$sub"/*.meta ; do
                [[ -f "$meta" ]] || continue
                # source the meta (simple key=value)
                local COMPILER TEST LOGTYPE SUBDIR DURATION_SEC STATUS RECORDS TEST_OUTPUT_MODE
                COMPILER=""; TEST=""; LOGTYPE=""; SUBDIR=""; DURATION_SEC=""; STATUS=""; RECORDS=""; TEST_OUTPUT_MODE=""
                # shellcheck disable=SC1090
                . "$meta" 2>/dev/null || true

                if [[ -z "$TEST_OUTPUT_MODE" ]]; then
                    TEST_OUTPUT_MODE="off"
                fi

                # Measure actual persist file size(s) on disk for this run
                local persist_bytes=0
                for f in "$logtype_dir/$SUBDIR"/persist* "$logtype_dir/$SUBDIR"/*.db "$logtype_dir/$SUBDIR"/*_direct.sql "$logtype_dir/$SUBDIR"/*.sql; do
                    if [[ -f "$f" ]]; then
                        local s
                        s=$(stat -c%s "$f" 2>/dev/null || echo 0)
                        persist_bytes=$((persist_bytes + s))
                    fi
                done
                local persist_human
                if command -v numfmt >/dev/null 2>&1; then
                    persist_human=$(numfmt --to=iec-i --suffix=B --format="%.1f" "$persist_bytes" 2>/dev/null || echo "${persist_bytes}B")
                else
                    persist_human="${persist_bytes}B"
                fi
                local persist_mbs="N/A"
                if [[ "$DURATION_SEC" =~ ^[0-9]+$ && "$DURATION_SEC" -gt 0 && "$persist_bytes" -gt 0 ]]; then
                    persist_mbs=$(echo "scale=1; $persist_bytes / 1024 / 1024 / $DURATION_SEC" | bc -l 2>/dev/null || echo "N/A")
                elif [[ "$persist_bytes" -gt 0 && "$DURATION_SEC" == "0" ]]; then
                    persist_mbs="instant"
                fi

                local rel_path="$RESULTS_REL_PREFIX/$(basename "$logtype_dir")/$SUBDIR/${COMPILER}.log"
                local log_link="[log*]($rel_path)"

                local display_ltype=${LOGTYPE}
                if [[ "$LOGTYPE" == "none" ]]; then display_ltype="inmem"; fi

                rowkey="${COMPILER}|${SUBDIR}|${LOGTYPE}|${TEST_OUTPUT_MODE}"
                if [[ -z "${seen[$rowkey]:-}" ]]; then
                    seen[$rowkey]=1
                    rows+=("| ${COMPILER} | ${SUBDIR} | ${display_ltype} | ${TEST_OUTPUT_MODE} | ${RECORDS:-?} | ${DURATION_SEC}s | ${persist_human} | ${persist_mbs} | ${STATUS} | ${log_link} |")
                fi

                # for faster + smaller computation: store b_dur b_size b_rec j_dur j_size j_rec per output mode
                local key="${SUBDIR}|${COMPILER}|${TEST_OUTPUT_MODE}"
                local cur="${scenario_data[$key]:-}"
                local b_dur="" b_size="" b_rec=""
                local j_dur="" j_size="" j_rec=""
                if [[ -n "$cur" ]]; then
                    read -r b_dur b_size b_rec j_dur j_size j_rec <<< "$cur"
                fi
                if [[ "$LOGTYPE" == "binary" ]]; then
                    b_dur="${DURATION_SEC}"
                    b_size="${persist_bytes}"
                    b_rec="${RECORDS}"
                elif [[ "$LOGTYPE" == "jtext" ]]; then
                    j_dur="${DURATION_SEC}"
                    j_size="${persist_bytes}"
                    j_rec="${RECORDS}"
                elif [[ "$LOGTYPE" == "sql" ]]; then
                    # treat sql like jtext for comparison
                    j_dur="${DURATION_SEC}"
                    j_size="${persist_bytes}"
                    j_rec="${RECORDS}"
                else
                    # none/inmem
                    j_dur="${DURATION_SEC}"
                    j_size="${persist_bytes}"
                    j_rec="${RECORDS}"
                fi
                scenario_data[$key]="${b_dur} ${b_size} ${b_rec} ${j_dur} ${j_size} ${j_rec}"
            done
        done
    done

    # Fill missing combinations from persist files so summary is never "short"
    for tname in "${!KNOWN_TESTS[@]}"; do
        for pdir in binary_logs jText_logs sql_logs inmem_logs; do
            local ltype=${KNOWN_PERSIST[$pdir]}
            for om in on off; do
                local rkey="${COMPILER}|${tname}|${ltype}|${om}"
                if [[ -z "${seen[$rkey]:-}" ]]; then
                    seen[$rkey]=1
                    local rec=${KNOWN_TESTS[$tname]}
                    local pdir_path="$results_base/$pdir/$tname"
                    local pbytes=0
                    for f in "$pdir_path"/persist* ; do
                        if [[ -f "$f" ]]; then
                            local s
                            s=$(stat -c%s "$f" 2>/dev/null || echo 0)
                            pbytes=$((pbytes + s))
                        fi
                    done
                    local phuman
                    if command -v numfmt >/dev/null 2>&1; then
                        phuman=$(numfmt --to=iec-i --suffix=B --format="%.1f" "$pbytes" 2>/dev/null || echo "${pbytes}B")
                    else
                        phuman="${pbytes}B"
                    fi
                    local dur="N/A"
                    # aggressive parse: look in any log in the subdir for duration for this test
                    local d
                    d=$(grep -h -oE "duration=[0-9]+s" "$pdir_path"/*${COMPILER}*.log 2>/dev/null | head -1 | grep -oE "[0-9]+" || echo "")
                    if [[ -z "$d" ]]; then
                        # fallback to any duration in subdir logs
                        d=$(grep -h -oE "duration=[0-9]+s" "$pdir_path"/*.log 2>/dev/null | head -1 | grep -oE "[0-9]+" || echo "")
                    fi
                    [[ -n "$d" ]] && dur="${d}s"
                    local pmbs="N/A"
                    if [[ "$dur" != "N/A" && "$pbytes" -gt 0 ]]; then
                        pmbs=$(echo "scale=1; $pbytes / 1024 / 1024 / ${dur%s}" | bc -l 2>/dev/null || echo "N/A")
                    fi
                    local rlog="$RESULTS_REL_PREFIX/$pdir/$tname/${COMPILER}_${ltype}_${om}.log"
                    rows+=("| ${COMPILER} | ${tname} | ${ltype} | ${om} | ${rec} | ${dur} | ${phuman} | ${pmbs} | ? | [log*]($rlog) |")

                    # populate scenario_data for the faster section too
                    skey="${tname}|${COMPILER}|${om}"
                    cur="${scenario_data[$skey]:-}"
                    read -r cb_dur cb_size cb_rec cj_dur cj_size cj_rec <<< "$cur" 2>/dev/null || { cb_dur=""; cb_size=""; cb_rec=""; cj_dur=""; cj_size=""; cj_rec=""; }
                    if [[ "$ltype" == "binary" ]]; then
                        cb_dur="$dur"
                        cb_size="$pbytes"
                        cb_rec="$rec"
                    elif [[ "$ltype" == "jtext" || "$ltype" == "sql" ]]; then
                        cj_dur="$dur"
                        cj_size="$pbytes"
                        cj_rec="$rec"
                    else
                        # none/inmem
                        cj_dur="$dur"
                        cj_size="$pbytes"
                        cj_rec="$rec"
                    fi
                    scenario_data[$skey]="$cb_dur $cb_size $cb_rec $cj_dur $cj_size $cj_rec"
                fi
            done
        done
    done

    # Main table
    cat >> "$out_file" <<'TABLE'

## Test Run Summary (all scenarios)

* = Not available on GitHub, folder too large for GitHub

| Compiler | Test | Log Type | Output | Records | Duration | Size | Rate | Status | Log |
|----------|------|----------|--------|---------|----------|------|------|--------|-----|
TABLE

    # Sort by Test (k3), then Log Type (k4), then Output (k5), then Compiler (k2).
    # "on" before "off" for nicer ordering within each (test+logtype).
    # This puts clang + gcc for the *exact same* (test + logtype + output) right next to each other.
    # E.g.
    # clang TS_STORE_TEST_001_TS binary on ...
    # gcc   TS_STORE_TEST_001_TS binary on ...
    # clang TS_STORE_TEST_001_TS binary off ...
    # gcc   TS_STORE_TEST_001_TS binary off ...
    # ... then jtext, then next test, etc.
    printf "%s\n" "${rows[@]}" | \
      sed 's/| on |/| 0on |/g; s/| off |/| 1off |/g' | \
      sort -t '|' -k3,3 -k4,4 -k5,5 -k2,2 | \
      sed 's/| 0on |/| on |/g; s/| 1off |/| off |/g' >> "$out_file"

    echo "* = Not available on GitHub, folder too large for GitHub" >> "$out_file"

    cat >> "$out_file" <<'FOOT1'

**Size / Rate**: total bytes written to persist files on disk (binary = single `.bin`; jText = `.jtext` + `_Ints.jtext` + `_Floats.jtext`). Rate = size / full run wall time. "instant" or N/A for zero-duration runs.

FOOT1

    cat >> "$out_file" <<EOF
**Total scenarios this run**: $total_scenarios (passed: $passed, failed: $failed)
EOF

    # Faster / smaller per scenario (per compiler + output mode)
    cat >> "$out_file" <<'FASTER'

## Which log type was faster / smaller? (per test + compiler + output mode)
(time = full binary wall time including async persist drain; size = total on-disk persist artifacts)

FASTER

    local any_comp=0
    declare -a faster_lines=()
    for key in "${!scenario_data[@]}"; do
        any_comp=1
        # key = SUBDIR|COMPILER|OUTPUT_MODE
        IFS='|' read -r tname cname omode <<< "$key"
        local data="${scenario_data[$key]}"
        read -r bdur bsize brec jdur jsize jrec <<< "$data"

        local time_line=""
        if [[ "$bdur" =~ ^[0-9]+$ && "$jdur" =~ ^[0-9]+$ ]]; then
            if (( bdur < jdur )); then
                time_line="**binary** faster (${bdur}s vs ${jdur}s"
            elif (( jdur < bdur )); then
                time_line="**jText** faster (${jdur}s vs ${bdur}s"
            else
                time_line="tie (${bdur}s"
            fi
        fi

        local size_line=""
        if [[ "$bsize" =~ ^[0-9]+$ && "$jsize" =~ ^[0-9]+$ && "$bsize" -gt 0 && "$jsize" -gt 0 ]]; then
            local delta_mb=$(( (bsize > jsize ? bsize - jsize : jsize - bsize) / 1024 / 1024 ))
            if (( bsize < jsize )); then
                size_line=", binary smaller by ${delta_mb}MB"
            elif (( jsize < bsize )); then
                size_line=", jText smaller by ${delta_mb}MB"
            else
                size_line=", same size"
            fi
        fi

        local rec_info="~${brec:-?} rec"
        if [[ -n "$time_line" || -n "$size_line" ]]; then
            faster_lines+=("- **$tname** ($cname, output=$omode): ${time_line}${size_line}) | ${rec_info}")
        fi
    done
    if (( any_comp == 0 )); then
        summary_entry "$out_file" "(No comparative data yet — run for both gcc and clang.)"
    else
        # Gather + sort so that for each (test, output), the clang and gcc lines are adjacent.
        # "on" before "off". Matches the main table ordering.
        declare -a keyed_faster=()
        for line in "${faster_lines[@]}"; do
            # Parse tname, cname, omode reliably (grep -o is used elsewhere in the script)
            tname=$(echo "$line" | grep -o 'TS_STORE_TEST_[^ *]*' | head -1)
            cname=$(echo "$line" | grep -o '([a-z]*,' | tr -d '(,' )
            omode_raw=$(echo "$line" | grep -o 'output=[^)]*' | cut -d= -f2 )
            # Use 0on/1off so "on" sorts before "off"
            if [[ "$omode_raw" == "on" ]]; then
                omode="0on"
            else
                omode="1off"
            fi
            keyed_faster+=("${tname}|${omode}|${cname}|${line}")
        done
        while IFS= read -r line || [[ -n "$line" ]]; do
            summary_entry "$out_file" "$line"
        done < <(printf "%s\n" "${keyed_faster[@]}" | sort -t'|' -k1,1 -k2,2 -k3,3 | cut -d'|' -f4-)
    fi

    # Notes + Configs
    cat >> "$out_file" <<'NOTES'

## Notes
- All tests now attach a `DoubleBufferedWriter` + chosen sink (`BinaryEventSink` or `JTextEventSink`) for asynchronous background persistence. Hot path remains fast.
- Persist artifacts (`.bin` or the 3 `.jtext` files) are written using the `--base-name` passed by the runner so they land inside the corresponding `test-results/$DISK_TYPE/*/TS_STORE_TEST_.../` subdirectory.
- `Records` are best-effort parsed from test output (tests now use reduced counts ~100 events for SSD longevity per matrix * instructions; old 1M-scale numbers were for prior full-stress runs). Inmem/none shows pure in-memory (no persist). SQL shows direct DB writes + debug INSERT file.
- Size and Rate columns: measured on-disk persist artifact size and effective MB/s (size / full test duration). The "Log" column links to the captured stdout for that run.
- Each (test, persisttype) is executed twice: once with output=on (live console, ANSI colors enabled via --color=1) and once with output=off (silent capture, --color=0). Persist types: binary, jtext, sql (direct), none (in-memory only). This shows the overhead of console output. Live "on" runs are colorful and pretty; saved logs are always plain text (ANSI stripped).
- Per-compiler times show build + full test suite execution time for that compiler (120 scenarios). Total suite duration is wall time across all compilers.
- Default (no --compiler or --compiler all) runs gcc then clang internally in one invocation. Use --compiler gcc|clang to run just one. The summary table and faster comparisons will include all compilers' data.
- Separate `TS_STORE_InMemory_Summary.md` is also generated for pure in-memory runs (the "none" scenario) of the heavy tests (detailed internal rate reporting).
- Separate `TS_STORE_SQL_Roundtrip_Summary.md` is generated for the post-test jText → SQL roundtrips (via CLI tools: emit .sql + load + queries + export-back) that run after every jtext persist scenario. .sql + *_fulltrip artifacts are co-located with jText logs for size/timing (this is jText-mediated, not native direct INSERTs from ts_store).
- The main summary now includes rows for all 4 persist types (binary, jtext, sql, inmem/none).
- Legacy `results/<compiler>/` tree may still exist from prior versions; new canonical location is `test-results/<DISK_TYPE>/`.

## Config settings used by the tests (LogConfig)
- 001/002/003/006/007 (TS): `ts_store_config<true, 6, 20, 43, 9, 6, false>` (size from test_params.txt / --threads etc; smoke 5×20, full high)
- 001/002/003/006/007 (XS): `ts_store_config<false, 6, 20, 43, 9, 6, false>`
- 004 (TS): `ts_store_config<true, 6, 20, 75, 9, 6, false>` (main) + result config (size configurable)
- 004 (XS): `ts_store_config<false, 6, 20, 75, 9, 6, false>`
- 005 (TS): `ts_store_config<true, 6, 20, 43, 9, 6, false>` (size from test_params.txt / --threads etc; smoke 5×20×1, full 250×4000×50 etc)
- 005 (XS): `ts_store_config<false, 6, 20, 43, 9, 6, false>`
- flags: standalone `TsStoreFlags` unit test (no store / no persist)

See main [README.md](README.md) and the test sources under `tests/` for details.

NOTES

    # Post-process to format numbers regional-aware (locale grouping e.g. 1,234,567 or 1.234.567 as appropriate per system locale)
    python3 -c '
import locale, re, sys
locale.setlocale(locale.LC_ALL, "")
with open(sys.argv[1]) as f: content = f.read()
def fmt_num(m):
    n = int(m.group(0))
    try:
        return locale.format_string("%d", n, grouping=True)
    except Exception:
        return "{:,}".format(n)
content = re.sub(r"\b\d{4,}\b", fmt_num, content)
with open(sys.argv[1], "w") as f: f.write(content)
' "$out_file"

    echo "Summary updated: $out_file"
}

COMPILER="all"
OUTPUT_MODE="yes"  # yes = live console (with ANSI colors for test output), no = silent (logs only)

usage() {
    echo "Usage: $0 [--compiler gcc|clang|all] [--output yes|no] [--disk x7k|10k|ssd] [--os-id OS_001]"
    echo "  Default: all (runs gcc then clang internally, one summary at end)"
    echo "  --output yes : live console output with ANSI colors (pretty)"
    echo "  --output no  : silent (logs only)"
    echo "  --disk x7k|10k|ssd : selects storage type (3-char for alignment)"
    echo "  --os-id OS_001     : use OS_001/OS_002... (see test_params.txt). Padded form (OS_%03d) recommended for scale."
    echo "  Creates: test-results/OS_001/<disk>/<size>/  (real OS name in OS_INFO.txt + central OS_MAP.txt)"
    echo "  This supports cross-OS / cross-disk testing while keeping the 3-char disk (x7k/10k/ssd) and 5-char size dirs vertically aligned."
    echo "  Additionally runs pure in-memory (no logs) for 005/007 and produces separate TS_STORE_InMemory_Summary.md with compile times."
    echo "  Runs selected persist types (binary, jtext, sql direct with debug INSERT file, none/inmem) + post roundtrips. Controlled by tests/test_params.txt (SIZE=smoke|full, [x] tests, DISK_TYPE, optional OS_ID for override). Uses padded OS_00n form by default for long-term scale."
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --compiler)
            COMPILER="$2"
            shift 2
            ;;
        --output)
            OUTPUT_MODE="$2"
            shift 2
            ;;
        --disk)
            DISK_TYPE="$2"
            shift 2
            ;;
        --os-id)
            OS_ID="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# After CLI overrides, ensure DISK_TYPE is set (CLI takes precedence over params file)
if [[ -z "$DISK_TYPE" ]]; then
    echo "ERROR: DISK_TYPE not set (use --disk x7k|10k|ssd or DISK_TYPE=... in tests/test_params.txt)"
    exit 1
fi

# Normalize common variations so people don't have to remember exact case/spelling
# Actual directories will be x7k / 10k / ssd (all exactly 3 chars) so they line up vertically when listed
case "${DISK_TYPE,,}" in   # ,, makes lowercase
    7k|7200|x7k|x7200)   DISK_TYPE=x7k ;;
    10k|10000|x10k|x10000) DISK_TYPE=10k ;;
    ssd|solidstate|xssd) DISK_TYPE=ssd ;;
    x7k|10k|ssd)         ;;  # already good
    *) echo "Warning: unknown DISK_TYPE '$DISK_TYPE' - using as-is. Recommended values: x7k, 10k, ssd";;
esac

if [[ "$OUTPUT_MODE" != "yes" && "$OUTPUT_MODE" != "no" ]]; then
    echo "--output must be yes or no"
    usage
fi

if [[ "$COMPILER" == "all" || -z "$COMPILER" ]]; then
    COMPILER_LIST=(gcc clang)
elif [[ "$COMPILER" == "gcc" ]]; then
    COMPILER_LIST=(gcc)
elif [[ "$COMPILER" == "clang" ]]; then
    COMPILER_LIST=(clang)
else
    echo "Compiler must be gcc, clang, or all (default)"
    usage
fi

# --------------------------------------------------------------------
# Auto OS_ID resolution using uname/os-release (the user does NOT pick it).
# We parse the OS, look up in test-results/OS_MAP.txt for a matching entry
# (OS_001, OS_002, ... padded), or assign the next free one (using OS_%03d)
# and append to the map.
# This keeps the short OS_00n dirs for alignment (and scales to hundreds of
# distinct OSes if the framework is successful across many machines/distros).
# While the map + per-run OS_INFO.txt record the real OS.
# If OS_ID was already set (via params or --os-id), we respect the override
# and still ensure the map has an entry.
# --------------------------------------------------------------------
if [[ -z "$OS_ID" ]]; then
  # Detect a pretty human name
  os_pretty=""
  if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release 2>/dev/null || true
    os_pretty="${PRETTY_NAME:-${NAME:-} ${VERSION_ID:-}}"
  fi
  if [[ -z "$os_pretty" ]]; then
    os_pretty=$(uname -s -r 2>/dev/null || echo "unknown-os")
  fi

  # Normalized key for fuzzy matching
  os_key=$(echo "$os_pretty" | tr '[:upper:]' '[:lower:]' | sed -E 's/[[:space:]]+/ /g; s/^ +| +$//g')

  MAP_FILE="$PROJECT_ROOT/test-results/OS_MAP.txt"
  mkdir -p "$(dirname "$MAP_FILE")"

  if [[ -f "$MAP_FILE" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      # Expect lines like "OS_001 = RHEL 9.6" or "OS_002 = RHEL 10.2"
      if [[ "$line" =~ ^(OS[0-9]+)[[:space:]]*=[[:space:]]*(.+)$ ]]; then
        map_id="${BASH_REMATCH[1]}"
        map_val="${BASH_REMATCH[2]}"
        map_key=$(echo "$map_val" | tr '[:upper:]' '[:lower:]' | sed -E 's/[[:space:]]+/ /g; s/^ +| +$//g')
        if [[ "$os_key" == *"$map_key"* || "$map_key" == *"$os_key"* ]]; then
          OS_ID="$map_id"
          break
        fi
      fi
    done < "$MAP_FILE"
  fi

  if [[ -z "$OS_ID" ]]; then
    # Find the highest existing OS number (OS_001, OS_002, ... style)
    max=0
    if [[ -f "$MAP_FILE" ]]; then
      while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ ^OS_([0-9]+)[[:space:]]*=[[:space:]] ]]; then
          num=${BASH_REMATCH[1]}
          # treat as decimal (strip leading zeros)
          num=$((10#$num))
          (( num > max )) && max=$num
        fi
      done < "$MAP_FILE"
    fi
    next=$((max + 1))
    OS_ID=$(printf 'OS_%03d' "$next")
    echo "$OS_ID = $os_pretty" >> "$MAP_FILE"
    echo "Auto-assigned $OS_ID for new OS '$os_pretty' → $MAP_FILE"
  else
    echo "Matched existing $OS_ID for OS '$os_pretty'"
  fi
else
  echo "Using user-provided/override OS_ID=$OS_ID"
fi

# test-results/ layout per disk type (define after params load + CLI overrides)
# Supports OS_ID (OS_001, OS_002 ... padded) + 3-char DISK_TYPE + 5-char SIZE_LABEL
if [[ -n "$OS_ID" ]]; then
    TEST_RESULTS_BASE="$PROJECT_ROOT/test-results/$OS_ID/$DISK_TYPE/$SIZE_LABEL"
else
    TEST_RESULTS_BASE="$PROJECT_ROOT/test-results/$DISK_TYPE"
fi
BINARY_LOGS="$TEST_RESULTS_BASE/binary_logs"
JTEXT_LOGS="$TEST_RESULTS_BASE/jText_logs"
SQL_LOGS="$TEST_RESULTS_BASE/sql_logs"
INMEM_LOGS="$TEST_RESULTS_BASE/inmem_logs"
INMEM_DIR="$TEST_RESULTS_BASE/inmem"
mkdir -p "$BINARY_LOGS"
mkdir -p "$JTEXT_LOGS"
mkdir -p "$SQL_LOGS"
mkdir -p "$INMEM_LOGS"
mkdir -p "$INMEM_DIR"

# Write OS / run metadata into the leaf directory.
# The visible cross-machine list is in test-results/OS_MAP.txt at the top.
# This OS_INFO.txt has the full details for *this* run (including the OS_ID assignment).
{
    echo "OS_ID: ${OS_ID:-<none>}"
    echo "DISK_TYPE: $DISK_TYPE"
    echo "SIZE: $SIZE"
    echo "SIZE_LABEL: $SIZE_LABEL"
    echo "Run started: $(date -u +"%Y-%m-%d %H:%M:%S UTC")"
    echo ""
    # Re-detect clean pretty name
    os_pretty=""
    if [[ -r /etc/os-release ]]; then
      . /etc/os-release 2>/dev/null || true
      os_pretty="${PRETTY_NAME:-${NAME:-} ${VERSION_ID:-}}"
    fi
    if [[ -z "$os_pretty" ]]; then
      os_pretty=$(uname -s -r 2>/dev/null || echo "unknown")
    fi
    echo "OS_NAME: $os_pretty"
    echo ""
    echo "=== uname -a ==="
    uname -a 2>/dev/null || true
    echo ""
    if [[ -r /etc/os-release ]]; then
        echo "=== /etc/os-release ==="
        cat /etc/os-release
    fi
    echo ""
    echo "=== hostname ==="
    hostname 2>/dev/null || true
    echo ""
    echo "See ../../OS_MAP.txt for the global OS_001=... list across machines."
} > "$TEST_RESULTS_BASE/OS_INFO.txt"
echo "OS metadata written to $TEST_RESULTS_BASE/OS_INFO.txt"

# Ensure the central visible OS_MAP.txt has this OS_ID (the auto-detection
# logic above already created the entry if it was a new OS).
if [[ -n "$OS_ID" ]]; then
    MAP_FILE="$PROJECT_ROOT/test-results/OS_MAP.txt"
    mkdir -p "$(dirname "$MAP_FILE")"
    if [[ ! -f "$MAP_FILE" ]] || ! grep -q "^$OS_ID =" "$MAP_FILE" 2>/dev/null; then
        echo "$OS_ID = $os_pretty" >> "$MAP_FILE"
        echo "Updated central OS map: $MAP_FILE  ($OS_ID = $os_pretty)"
    fi
fi

# Repo-root relative prefix for links inside the Markdown summaries
# (so they remain correct even with OS_ID / SIZE_LABEL nesting)
if [[ -n "$OS_ID" ]]; then
    RESULTS_REL_PREFIX="test-results/$OS_ID/$DISK_TYPE/$SIZE_LABEL"
else
    RESULTS_REL_PREFIX="test-results/$DISK_TYPE"
fi

echo "=== ts_store All Stress Tests Runner (double-buffered persist for ALL tests) [DISK_TYPE=$DISK_TYPE] ==="
echo "Compilers: ${COMPILER_LIST[*]}"
echo "Output mode: $OUTPUT_MODE (live console + ANSI colors when 'yes')"
echo "Primary output: $TEST_RESULTS_BASE/ (binary_logs + jText_logs + ...) + promoted summaries under test-summary/"
echo "  (OS_ID=$OS_ID DISK_TYPE=$DISK_TYPE SIZE_LABEL=$SIZE_LABEL -- see OS_INFO.txt in the leaf + OS_MAP.txt at top level)"
echo "Each test subdir will contain separate logs for output=on vs output=off."
echo "Each (test, logtype) is run twice per compiler: once with output=on (live console + ANSI colors), once with output=off (silent, no color)."

RUN_PASSED_ALL=0
RUN_FAILED_ALL=0
TOTAL_ALL=0

declare -A COMPILER_TEST_DURATIONS=()
declare -A COMPILER_BUILD_DURATIONS=()

# Start timing the entire suite (all compilers + tests + builds)
SUITE_START_TS=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
SUITE_START_EP=$(date +%s)

for COMPILER in "${COMPILER_LIST[@]}"; do
  if [[ "$COMPILER" == "gcc" ]]; then
    COMPILER_DISPLAY="GCC 15 (gcc-toolset-15)"
  else
    COMPILER_DISPLAY="Clang 21.1 (Red Hat)"
  fi

  echo "=== Running for compiler: $COMPILER ==="

  # Clean previous run's log/meta files for *this* compiler (keep data from other compilers)
  echo "Cleaning previous $COMPILER artifacts from test-results/$DISK_TYPE/..."
  for logdir in "$BINARY_LOGS" "$JTEXT_LOGS" "$SQL_LOGS" "$INMEM_LOGS"; do
    for sub in "$logdir"/TS_STORE_TEST_* ; do
      [ -d "$sub" ] || continue
      rm -f "$sub/${COMPILER}_"*.log "$sub/${COMPILER}_"*.meta 2>/dev/null || true
      # Also remove previous persist artifacts so re-runs always produce files with current
      # standardized //File Name / //Date / //Purpose headers (for jtext/sql/bin + field lists).
      rm -f "$sub"/{persist,persist_Ints,persist_Floats}.{bin,jtext,sql,db} 2>/dev/null || true
    done
  done

  # Clean previous inmem for this compiler (the inmem runs are always fresh for the current run)
  rm -f "$INMEM_DIR/${COMPILER}_"* 2>/dev/null || true

  # Determine build dir and how to invoke cmake/build
BUILD_DIR="$PROJECT_ROOT/build-results-$COMPILER"
CMAKE_CMD="cmake"
BUILD_CMD="cmake --build . --target"

if [[ "$COMPILER" == "gcc" ]]; then
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DTS_STORE_ENABLE_SQLITE_PERSIST=ON"
    # Enable jText for the big double-buffer tests (005/007) so they can use
    # JTextSplitEventLog via JTextEventSink, producing separate _Ints.jtext and
    # _Floats.jtext files with all the metric data (in addition to the main log).
    # The stress tests (001-007) are always built.
else
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON -DTS_STORE_ENABLE_SQLITE_PERSIST=ON -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld"
fi

echo "Using build dir: $BUILD_DIR"

# Only reconfigure/rebuild if the expected binaries are missing (to speed up repeated runs)
NEED_BUILD=0
for t in ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags; do
    if [[ ! -x "$BUILD_DIR/$t" ]]; then
        NEED_BUILD=1
        break
    fi
done

# Force rebuild if key sources (tests, CMake, options parser) are newer than the binaries.
# This ensures the new --persist / double-buffer attach code is compiled in for all tests.
# Also watch the tunable heavy tests (005/007) so size reductions for SSD etc. force rebuild of the right bins.
for src in "$PROJECT_ROOT/tests/ts_store_001/test_001_TS.cpp" \
           "$PROJECT_ROOT/CMakeLists.txt" \
           "$PROJECT_ROOT/include/beman/ts_store/ts_store_headers/impl_details/test_options.hpp" \
           "$PROJECT_ROOT/tests/ts_store_005/test_005_TS.cpp" \
           "$PROJECT_ROOT/tests/ts_store_005/test_005_XS.cpp" \
           "$PROJECT_ROOT/tests/ts_store_007/test_007_TS.cpp" \
           "$PROJECT_ROOT/tests/ts_store_007/test_007_XS.cpp" ; do
    if [[ -f "$src" && -f "$BUILD_DIR/ts_store_001_TS" && "$src" -nt "$BUILD_DIR/ts_store_001_TS" ]]; then
        NEED_BUILD=1
        echo "Source newer than existing binaries ($src) — forcing clean build"
        break
    fi
done

BUILD_START_TS=""
BUILD_END_TS=""
BUILD_DURATION_SEC="skipped"

if [[ $NEED_BUILD -eq 1 ]]; then
    echo "Binaries missing or first run — cleaning and building..."
    BUILD_START_TS=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
    BUILD_START_EP=$(date +%s)
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    CONFIGURE_CMD="$CMAKE_CMD $CMAKE_ARGS '$PROJECT_ROOT'"

    if [[ "$COMPILER" == "gcc" ]]; then
        scl enable gcc-toolset-15 -- bash -c "$CONFIGURE_CMD"
    else
        eval "$CONFIGURE_CMD"
    fi

    echo "Building all stress test binaries..."
    TARGETS="ts_store_001_TS ts_store_001_XS ts_store_002_TS ts_store_002_XS ts_store_003_TS ts_store_003_XS ts_store_004_TS ts_store_004_XS ts_store_005_TS ts_store_005_XS ts_store_006_TS ts_store_006_XS ts_store_007_TS ts_store_007_XS ts_store_flags"

    if [[ "$COMPILER" == "gcc" ]]; then
        scl enable gcc-toolset-15 -- bash -c "cmake --build . --target $TARGETS -j\$(nproc)"
    else
        cmake --build . --target $TARGETS -j$(nproc)
    fi
    BUILD_END_TS=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
    BUILD_END_EP=$(date +%s)
    BUILD_DURATION_SEC=$((BUILD_END_EP - BUILD_START_EP))
    echo "Build complete. (duration: ${BUILD_DURATION_SEC}s)"
else
    echo "All test binaries already present in $BUILD_DIR — skipping rebuild."
    cd "$BUILD_DIR"
fi

COMPILER_BUILD_DURATIONS["$COMPILER"]=$BUILD_DURATION_SEC

TEST_START_EP=$(date +%s)

# === New structured run: every test x2 (binary + jtext), logs + persist artifacts under test-results/$DISK_TYPE/ ===

# Combinatorial matrix (the fields whose product gives the scenario count):
# 1. Compiler dimension (from COMPILER_LIST): "gcc", "clang"  (1 or 2; "all" runs both sequentially)
# 2. Test dimension (this list): 15 entries
#    - 001_TS / 001_XS ... 007_TS / 007_XS   (timestamps on vs off)
#    - flags
# 3. Persist/log type: "binary", "jtext"  (2)
# 4. Output mode: "on" (live console + ANSI colors via --color=1), "off" (silent redirect, --color=0)  (2)
#    Table sorted by Test, LogType, Output (on before off), Compiler (clang before gcc) for easy side-by-side comparison.
#
# Additionally: pure in-memory runs (no persistence) for the heavy 005/007 tests to measure the true in-memory hot path.
#
# Per-compiler scenarios (log runs): 15 × 2 × 2 = 60
# When running "all" (the default): 2 compilers × 60 = 120 total log scenarios + in-memory for heavy tests.
# Compile times are captured per compiler and included in both summaries.
#
# What each scenario produces:
# - Runner capture: ${COMPILER}_${logtype}_${mode}.log   (stdout of the test binary)
# - Meta data:       ${COMPILER}_${logtype}_${mode}.meta (duration, records, persist size, status, etc.)
# - Persist artifacts: the actual on-disk logs written by the test
#     (persist.bin  or  persist.jtext + persist_Ints.jtext + persist_Floats.jtext)
#     placed in the TS_STORE_TEST_... subdir via --base-name
#
# Subdirectory layout (one per test dimension value):
#   test-results/OS_001/$DISK_TYPE/<SIZE_LABEL>/binary_logs/TS_STORE_TEST_00N_XX/
# (TESTS array is built above from test_params.txt or defaults to all)

echo
echo "=== Running all tests (double-buffered async persist for every test) ==="
echo "Binary logs : $BINARY_LOGS/"
echo "jText logs  : $JTEXT_LOGS/"
echo "Combinatorics: ${#TESTS[@]} tests × 4 persist types (binary, jtext, sql direct, none/inmem) × 2 output modes (from test_params.txt)"
echo "SIZE=$SIZE (smoke ~100 records or full); sizes: threads=$THREADS events=$EVENTS_PER_THREAD runs=$RUNS"
echo "Each (test, logtype) is run twice per compiler: once with output=on (live console + ANSI colors), once with output=off (silent, no color)."
echo "Additionally: pure in-memory (no persistence) runs for 005/007 throughput tests."
echo

TOTAL_SCENARIOS=$(( ${#TESTS[@]} * 4 * 2 ))  # from config selection × 4 persist × 2 output modes
RUN_PASSED=0
RUN_FAILED=0

# Per-compiler build info for the summary (last compiler's for the old BUILD_INFO field)
if [[ "$BUILD_DURATION_SEC" == "skipped" ]]; then
  BUILD_INFO="Build: skipped (binaries up to date)"
else
  BUILD_INFO="Build duration: ${BUILD_DURATION_SEC}s (start: ${BUILD_START_TS:-N/A}, end: ${BUILD_END_TS:-N/A})"
fi

for test in "${TESTS[@]}"; do
    bin="./$test"
    if [[ ! -x "$bin" ]]; then
        echo "ERROR: binary $bin not found"
        for persist_val in binary jtext; do
            for om in on off; do
                prepare_log_dir "$test" "$persist_val"
                echo "ERROR: binary $bin not found or not executable (logtype=$persist_val, output=$om)" > "$ldir/${COMPILER}_${persist_val}_${om}.log"
                echo "=== TEST RUN FAILED (logtype=$persist_val, output=$om) ===" >> "$ldir/${COMPILER}_${persist_val}_${om}.log"
            done
        done
        RUN_FAILED=$((RUN_FAILED + 4))
        continue
    fi

    for persist_val in binary jtext sql none; do
        logtype="$persist_val"
        prepare_log_dir "$test" "$persist_val"
        persist_base="$ldir/persist"

        for test_output_mode in on off; do
            run_log="$ldir/${COMPILER}_${logtype}_${test_output_mode}.log"

            echo "Running $test (logtype=$logtype, output=$test_output_mode) ... -> $run_log"

            start_ep=$(date +%s)

            if [[ "$test_output_mode" == "on" ]]; then
                # "on" = live console with ANSI colors enabled (pretty output visible)
                set +e
                "$bin" --interactive=0 --color=1 --persist="$logtype" --base-name="$persist_base" \
                    --threads=$THREADS --events-per-thread=$EVENTS_PER_THREAD --runs=$RUNS < /dev/null 2>&1 \
                    | tee >(strip_ansi > "$run_log")
                bin_status=${PIPESTATUS[0]}
                set -e
            else
                # "off" = silent, force no color (clean for logs; we strip anyway)
                set +e
                "$bin" --interactive=0 --color=0 --persist="$logtype" --base-name="$persist_base" \
                    --threads=$THREADS --events-per-thread=$EVENTS_PER_THREAD --runs=$RUNS < /dev/null 2>&1 \
                    | strip_ansi > "$run_log"
                bin_status=${PIPESTATUS[0]}
                set -e
            fi

            stop_ep=$(date +%s)
            dur_sec=$((stop_ep - start_ep))

            if [ $bin_status -ne 0 ]; then
                echo "  -> $test / $logtype / $test_output_mode FAILED (${dur_sec}s)"
                echo "=== TEST RUN FAILED (logtype=$logtype, output=$test_output_mode, duration=${dur_sec}s) ===" >> "$run_log"
                RUN_FAILED=$((RUN_FAILED + 1))
            else
                echo "  -> $test / $logtype / $test_output_mode PASSED (${dur_sec}s)"
                echo "=== TEST RUN PASSED (logtype=$logtype, output=$test_output_mode, duration=${dur_sec}s) ===" >> "$run_log"
                RUN_PASSED=$((RUN_PASSED + 1))
            fi

            sql_post_start=0
            sql_post_dur=0
            sql_size_bytes=0
            fulltrip_size_bytes=0
            sql_rows_loaded=0
            if [[ ("$logtype" == "jtext" || "$logtype" == "sql") && $bin_status -eq 0 ]]; then
                sql_post_start=$(date +%s)
                if [[ "$logtype" == "jtext" ]]; then
                    echo "  -> post-test: jText → SQL roundtrip (via CLI tools), for $test ..."
                else
                    echo "  -> post-test: SQL → jText roundtrip (export DB to jtext), for $test ..."
                fi
                # Note: "sql" persist is native direct-to-SQL from ts_store (SqlEventSink writes INSERTs).
                # "jtext" uses jText interchange + CLI for the SQL step (interim).
                JPROC="$JTEXT_PROCESS_BIN"
                J2S="$JAC_J2S_BIN"
                S2J="$JAC_S2J_BIN"
                if [[ "$logtype" == "jtext" ]]; then
                    DBF="/tmp/ts_${test}_${COMPILER}_${logtype}.db"
                    rm -f "$DBF"
                    for bname in persist persist_Ints persist_Floats; do
                        SQLF="$ldir/${bname}.sql"
                        if [[ -x "$JPROC" ]]; then
                            $JPROC "$ldir" $bname "$SQLF" >/dev/null 2>&1 || echo "    process $bname -> .sql failed"
                            sed -i "s|${ldir}/${bname}|test_${bname}|g" "$SQLF" 2>/dev/null || true
                        fi
                        if [[ -x "$J2S" ]]; then
                            $J2S "$ldir" $bname "$DBF" 2>&1 | tail -1 || echo "    j2s load $bname failed"
                        elif [[ -f "$SQLF" ]]; then
                            sqlite3 "$DBF" < "$SQLF" 2>&1 | tail -1 || true
                        fi
                    done
                else
                    # for sql persist, the sink wrote persist.db (and debug .sql)
                    DBF="$ldir/persist.db"
                fi
                # Queries
                echo "    Post-load queries:"
                for tbl in persist persist_Ints persist_Floats test_persist test_persist_Ints test_persist_Floats; do
                    cnt=$(sqlite3 "$DBF" "SELECT count(*) FROM $tbl;" 2>/dev/null || echo 0)
                    if [[ "$cnt" -gt 0 ]]; then
                        echo "      $tbl: $cnt rows"
                        sql_rows_loaded=$((sql_rows_loaded > cnt ? sql_rows_loaded : cnt))
                    fi
                done
                # Export to jtext for roundtrip (for both jtext and sql cases)
                echo "    Export roundtrip (to jText):"
                for bname in persist persist_Ints persist_Floats; do
                    tbls=("persist" "persist_Ints" "persist_Floats" "test_persist" "test_persist_Ints" "test_persist_Floats")
                    for tbl in "${tbls[@]}"; do
                        outdir="$ldir/${bname}_fulltrip"
                        mkdir -p "$outdir"
                        if [[ -x "$S2J" && -f "$DBF" ]]; then
                            if sqlite3 "$DBF" "SELECT 1 FROM $tbl LIMIT 1;" >/dev/null 2>&1; then
                                $S2J "$DBF" "$tbl" "$outdir" >/dev/null 2>&1 || echo "      export $tbl failed"
                                produced=$( (ls "$outdir"/* 2>/dev/null || true) | wc -l | tr -d ' ' || echo 0 )
                                echo "      $tbl -> $outdir/ ($produced files)"
                                break
                            fi
                        fi
                    done
                done
                sql_post_end=$(date +%s)
                sql_post_dur=$((sql_post_end - sql_post_start))
                # Capture sizes for SQL artifacts
                for f in "$ldir"/*.sql "$ldir"/*.db; do
                    [[ -f "$f" ]] && sql_size_bytes=$((sql_size_bytes + $(stat -c%s "$f" 2>/dev/null || echo 0)))
                done
                for d in "$ldir"/*_fulltrip; do
                    if [[ -d "$d" ]]; then
                        for ff in "$d"/*; do
                            [[ -f "$ff" ]] && fulltrip_size_bytes=$((fulltrip_size_bytes + $(stat -c%s "$ff" 2>/dev/null || echo 0)))
                        done
                    fi
                done
            fi

            # Write a tiny meta for easy summary scanning
            # Also capture persist size right after run (for cases where summary is regenerated later)
            pbytes=0
            for f in "$ldir"/persist*; do
                [[ -f "$f" ]] && pbytes=$((pbytes + $(stat -c%s "$f" 2>/dev/null || echo 0)))
            done
            cat > "$ldir/${COMPILER}_${logtype}_${test_output_mode}.meta" <<META
COMPILER=${COMPILER}
TEST=${test}
LOGTYPE=${logtype}
SUBDIR=${sdir}
TEST_OUTPUT_MODE=${test_output_mode}
DURATION_SEC=${dur_sec}
STATUS=$([ $bin_status -eq 0 ] && echo PASS || echo FAIL)
RECORDS=$(extract_record_count "$run_log")
PERSIST_SIZE_BYTES=${pbytes}
SQL_POST_DUR_SEC=${sql_post_dur}
SQL_SIZE_BYTES=${sql_size_bytes}
FULLTRIP_SIZE_BYTES=${fulltrip_size_bytes}
SQL_ROWS_LOADED=${sql_rows_loaded}
META
        done
    done
done
  # end of per-test loop for this compiler

  echo
  echo "=== All test scenarios completed for $COMPILER ==="
  echo "Passed: $RUN_PASSED / $TOTAL_SCENARIOS"
  echo "Failed: $RUN_FAILED / $TOTAL_SCENARIOS"
  echo "Structured logs + persist files under: $TEST_RESULTS_BASE/"
  echo

  TEST_END_EP=$(date +%s)
  TEST_DURATION_SEC=$((TEST_END_EP - TEST_START_EP))
  COMPILER_TEST_DURATIONS["$COMPILER"]=$TEST_DURATION_SEC
  echo "Test scenarios for $COMPILER complete. (duration: ${TEST_DURATION_SEC}s)"

  # Pure in-memory hot path (no persistence, no logs) for the throughput-heavy tests
  echo "=== Pure in-memory hot path (no persistence/logs) for $COMPILER ==="
  for heavy in ts_store_005_TS ts_store_005_XS ts_store_007_TS ts_store_007_XS; do
    if [[ ! -x "./$heavy" ]]; then
      continue
    fi
    # respect config selection for the additional inmem detailed run of heavy
    if [[ "$heavy" =~ ts_store_([0-9]{3}) ]]; then
      k=${BASH_REMATCH[1]}
      if [[ ${#SELECTED_TESTS[@]} -gt 0 && -z "${SELECTED_TESTS[$k]:-}" ]]; then
        continue
      fi
    fi
    echo "  Running $heavy pure in-memory..."
    inmem_log="$INMEM_DIR/${COMPILER}_${heavy}_inmem.log"
    start_im=$(date +%s.%N)
    ./${heavy} --persist=none --interactive=0 --color=0 \
        --threads=$THREADS --events-per-thread=$EVENTS_PER_THREAD --runs=$RUNS > "$inmem_log" 2>&1 || true
    end_im=$(date +%s.%N)
    im_dur=$(echo "$end_im - $start_im" | bc -l 2>/dev/null || echo "0")
    # Parse the actual scale and average from the test output (no longer assume 1M)
    scale_line=$(grep 'FINAL MASSIVE TEST — ' "$inmem_log" | tail -1 || echo "")
    # e.g. "=== FINAL MASSIVE TEST — 100 entries × 1 runs ==="
    entries=$(echo "$scale_line" | sed -n 's/.*— \([0-9,]*\) entries.*/\1/p' | tr -d ',' | head -1 || echo "0")
    runs=$(echo "$scale_line" | sed -n 's/.*× \([0-9]*\) runs.*/\1/p' | head -1 || echo "1")
    # The avg line has both us and the extrapolated ops/sec
    avg_line=$(grep 'Average            : ' "$inmem_log" | tail -1 || echo "")
    # e.g. "  Average            :       174 µs  →    574713 ops/sec"
    avg_us=$(echo "$avg_line" | sed -n 's/.*: *\([0-9]*\) µs.*/\1/p' | head -1 || echo "0")
    avg_ops=$(echo "$avg_line" | sed -n 's/.*→ *\([0-9,]*\) ops\/sec.*/\1/p' | tr -d ',' | head -1 || echo "0")
    printf "    Full in-mem run: %.3fs (scale: %s entries × %s runs, measured %.0f µs) → ~%s ops/sec (extrapolated)\n" \
      "$im_dur" "${entries:-?}" "${runs:-?}" "${avg_us:-0}" "${avg_ops:-0}"
    inmem_meta="$INMEM_DIR/${COMPILER}_${heavy}.meta"
    cat > "$inmem_meta" <<META
COMPILER=${COMPILER}
TEST=${heavy}
DURATION_SEC=${im_dur}
AVG_OPS_PER_SEC=${avg_ops}
TOTAL_EVENTS=${entries:-0}
RUNS=${runs:-1}
AVG_US=${avg_us:-0}
META
  done

  RUN_PASSED_ALL=$((RUN_PASSED_ALL + RUN_PASSED))
  RUN_FAILED_ALL=$((RUN_FAILED_ALL + RUN_FAILED))
  TOTAL_ALL=$((TOTAL_ALL + TOTAL_SCENARIOS))
done
# end for COMPILER_LIST

RUN_PASSED=$RUN_PASSED_ALL
RUN_FAILED=$RUN_FAILED_ALL
TOTAL_SCENARIOS=$TOTAL_ALL

if [[ ${#COMPILER_LIST[@]} -gt 1 ]]; then
  COMPILER_DISPLAY="all (${COMPILER_LIST[*]})"
  # BUILD_INFO below is from the last compiler; the table has data for all
fi

# Build per-compiler timing strings for summary
PER_COMPILER_TIMES=""
for c in "${COMPILER_LIST[@]}"; do
  bdur=${COMPILER_BUILD_DURATIONS[$c]:-skipped}
  tdur=${COMPILER_TEST_DURATIONS[$c]:-0}
  bhuman=$(format_duration "$bdur")
  thuman=$(format_duration "$tdur")
  label=$(printf "  %-7s" "$c:")
  if [[ "$bdur" == "skipped" ]]; then
    PER_COMPILER_TIMES+="${label}build $bhuman, tests $thuman"$'\n'
  else
    PER_COMPILER_TIMES+="${label}build $bhuman + tests $thuman"$'\n'
  fi
done

# Quick verification that the number of produced artifacts matches the combinatorics
META_COUNT=$(find "$TEST_RESULTS_BASE" -name '*.meta' | wc -l)
echo "Verification: expected $TOTAL_SCENARIOS scenarios, found $META_COUNT .meta files"

# Capture total wall time for the entire suite (including all compilers, builds, and runs)
SUITE_END_EP=$(date +%s)
SUITE_DURATION_SEC=$((SUITE_END_EP - SUITE_START_EP))

# Generate (or update) the rich combined summary document (table will contain data from all compilers)
SUMMARY_FILE="$TEST_RESULTS_BASE/TS_STORE_Test_Summary.md"
generate_test_summary "$SUMMARY_FILE" "$COMPILER" "$COMPILER_DISPLAY" "$BUILD_INFO" "$TEST_RESULTS_BASE" "$RUN_PASSED" "$RUN_FAILED" "$TOTAL_SCENARIOS" "$SUITE_DURATION_SEC" "$PER_COMPILER_TIMES"

echo "Rich summary written to: $SUMMARY_FILE"
echo
echo "Done."

# =============================================================================
# Separate In-Memory Hot Path Summary (pure in-memory, no logs/persistence)
# =============================================================================
echo "Generating separate in-memory hot path summary..."

INMEM_SUMMARY="$TEST_RESULTS_BASE/TS_STORE_InMemory_Summary.md"

# Re-get basic info (dupe small logic from generate for standalone)
inmem_now=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
inmem_os="unknown"
if [[ -r /etc/os-release ]]; then
    . /etc/os-release 2>/dev/null || true
    inmem_os="${PRETTY_NAME:-$NAME $VERSION}"
else
    inmem_os=$(uname -sr)
fi

cat > "$INMEM_SUMMARY" <<'IMHDR'
# TS_STORE In-Memory Hot Path Summary (no persistence / no logs) - the "none" / pure inmem scenario for heavy tests (detailed rate)

**Date**: $inmem_now  
**OS**: $inmem_os  
**Generated by**: scripts/run_all_tests.sh (pure in-memory runs with --persist=none)

## Purpose
These runs execute the core in-memory `save_event` hot path **without any persistence attached** (no jText, no Binary, no logs written).  
This gives the "pure" in-memory throughput for comparison against the persisted log runs in the main summary.

## Per-Compiler Compile Times + In-Memory Throughput
IMHDR

for c in "${COMPILER_LIST[@]}"; do
  bdur=${COMPILER_BUILD_DURATIONS[$c]:-skipped}
  bhuman=$(format_duration "$bdur")
  echo "### Compiler: $c" >> "$INMEM_SUMMARY"
  summary_entry "$INMEM_SUMMARY" "- Compile time: $bhuman"
  for h in ts_store_005_TS ts_store_005_XS ts_store_007_TS ts_store_007_XS; do
    m="$INMEM_DIR/${c}_${h}.meta"
    if [[ -f "$m" ]]; then
      # shellcheck disable=SC1090
      . "$m" 2>/dev/null || true
      aops=${AVG_OPS_PER_SEC:-?}
      idur=${DURATION_SEC:-?}
      ev=${TOTAL_EVENTS:-?}
      rn=${RUNS:-?}
      aus=${AVG_US:-?}
      # Use precise measured us if available, else the shell dur (for small runs dur may be 0)
      dur_display=${aus:-${idur}}
      if [[ "$aus" != "?" && "$aus" != "" ]]; then
        dur_display="${aus} µs"
      else
        dur_display="${idur}s"
      fi
      summary_entry "$INMEM_SUMMARY" "- ${h} (${ev} entries × ${rn} runs): ~${aops} ops/sec (measured ${dur_display})"
    fi
  done
  echo "" >> "$INMEM_SUMMARY"
done

cat >> "$INMEM_SUMMARY" <<'IMNOTES'

## Notes
- These are **pure in-memory** numbers (no logging, no double-buffer submit cost beyond the in-memory store work).
- Compare to main `TS_STORE_Test_Summary.md` (which includes async persistence) to see the cost of durable logging.
- Higher artificial rates are easy without recording (e.g. just a counter). The meaningful engineering number is sustained rate *while recording*.
- Compile times are the same as used for the log runs (binaries built once per compiler).
- The scale (N entries × M runs) and measured time come from the test binary itself (high_resolution_clock + verify). Rates are extrapolated to 1M-event equivalent for historical comparison, but actual run size is shown.
- In-memory runs use --persist=none (no DoubleBufferedWriter attached).

See main [TS_STORE_Test_Summary.md](TS_STORE_Test_Summary.md) for the full matrix with persistence (binary/jText, on/off, etc.).
IMNOTES

    # Regional number formatting for any large nums in inmem summary too
    python3 -c '
import locale, re, sys
locale.setlocale(locale.LC_ALL, "")
with open(sys.argv[1]) as f: content = f.read()
def fmt_num(m):
    n = int(m.group(0))
    try:
        return locale.format_string("%d", n, grouping=True)
    except Exception:
        return "{:,}".format(n)
content = re.sub(r"\b\d{4,}\b", fmt_num, content)
with open(sys.argv[1], "w") as f: f.write(content)
' "$INMEM_SUMMARY"

echo "In-memory summary written to: $INMEM_SUMMARY"

# =============================================================================
# SQL Roundtrip Summary (jText -> SQL direct via CLI + queries + export back)
# =============================================================================
echo "Generating SQL roundtrip summary (from jtext post-processing)..."

SQL_SUMMARY="$TEST_RESULTS_BASE/TS_STORE_SQL_Roundtrip_Summary.md"

sql_now=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
sql_os="unknown"
if [[ -r /etc/os-release ]]; then
    . /etc/os-release 2>/dev/null || true
    sql_os="${PRETTY_NAME:-$NAME $VERSION}"
else
    sql_os=$(uname -sr)
fi

cat > "$SQL_SUMMARY" <<'SQLHDR'
//File:    TS_STORE_SQL_Roundtrip_Summary.md
//Date:    $sql_now
//Purpose: SQL Roundtrip Summary File - documents jText -> SQL direct (CLI) loads, queries, and export-back roundtrips performed after every jtext persist test run
//Related: type=ts_store summary=SQL
//
// (This file is auto-generated by the test runner; the // header follows project jText standardization for all committed/generated files.)
# TS_STORE jText-to-SQL Roundtrip Summary (via CLI tools + export back)

**Date**: $sql_now  
**OS**: $sql_os  
**Generated by**: scripts/run_all_tests.sh (post-test for every jtext persist scenario)

## Purpose
**Important note** (per clarification): jText → SQL (via CLI tools) is **not** native "direct to SQL".

True direct-to-SQL would mean the ts_store executable (or a SqlEventSink attached via DoubleBufferedWriter) emitting INSERT statements / using prepared statements directly to a database at persist time, without going through an intermediate jText file.

What this summary covers is the current jText-mediated roundtrip mechanism used as an interim step to exercise "SQL direct" loads + after-test exports (matrix * points 3/4/6/7):
- jtext_process CLI to emit .sql companions (with canonical // headers)
- jtext_to_sqlite CLI to load jText data into SQLite (jText → SQL via interchange + tools)
- sqlite queries for row counts
- sqlite_to_jtext CLI to export the loaded data back to *_fulltrip/ jText (roundtrip verification)

This is performed after every jtext persist run (on/off). Artifacts (.sql, *_fulltrip dirs) live alongside the jText logs under test-results/$DISK_TYPE/jText_logs/TS_STORE_TEST_.../ so they are included for size and timing.

## Per-Compiler jText-to-SQL Post-Processing (load + export)
SQLHDR

for c in "${COMPILER_LIST[@]}"; do
  echo "### Compiler: $c" >> "$SQL_SUMMARY"
  echo "" >> "$SQL_SUMMARY"
  for sub in TS_STORE_TEST_* ; do
    for om in on off; do
      m="$JTEXT_LOGS/$sub/${c}_jtext_${om}.meta"
      if [[ -f "$m" ]]; then
        # shellcheck disable=SC1090
        . "$m" 2>/dev/null || true
        if [[ "${LOGTYPE:-}" == "jtext" ]]; then
          sql_dur=${SQL_POST_DUR_SEC:-0}
          sql_sz=${SQL_SIZE_BYTES:-0}
          ft_sz=${FULLTRIP_SIZE_BYTES:-0}
          rows=${SQL_ROWS_LOADED:-0}
          testn=${TEST:-$sub}
          if [[ "$sql_dur" -gt 0 || "$sql_sz" -gt 0 || "$rows" -gt 0 ]]; then
            # human sizes
            sql_h="0B"; ft_h="0B"
            if command -v numfmt >/dev/null 2>&1; then
              sql_h=$(numfmt --to=iec-i --suffix=B --format="%.1f" "$sql_sz" 2>/dev/null || echo "${sql_sz}B")
              ft_h=$(numfmt --to=iec-i --suffix=B --format="%.1f" "$ft_sz" 2>/dev/null || echo "${ft_sz}B")
            else
              sql_h="${sql_sz}B"; ft_h="${ft_sz}B"
            fi
            summary_entry "$SQL_SUMMARY" "- ${testn} / jtext / ${om} : post-dur ${sql_dur}s | .sql ${sql_h} | fulltrip ${ft_h} | rows ${rows}"
          fi
        fi
      fi
    done
  done
  echo "" >> "$SQL_SUMMARY"
done

cat >> "$SQL_SUMMARY" <<'SQLNOTES'

## Notes
- Timings and sizes captured in the runner's per-scenario .meta files (SQL_POST_DUR_SEC, SQL_SIZE_BYTES, FULLTRIP_SIZE_BYTES, SQL_ROWS_LOADED).
- .sql files are the companions generated by jtext_process (usable directly with sqlite3).
- fulltrip dirs contain the exported jText from the DB (for fidelity roundtrip checks).
- Queries are simple COUNT(*) per table (main + ints + floats splits).
- This is "after test" verification on top of the jtext persist runs; the core test binaries still use binary or jtext (or none for inmem).
- This is jText → SQL via CLI tools as an interim step. A real direct-to-SQL writer (INSERTs emitted straight from ts_store / a Sql sink) is still future work.

See main [TS_STORE_Test_Summary.md](TS_STORE_Test_Summary.md) for the full binary/jtext matrix and [README.md](README.md) for overall usage.
SQLNOTES

    # Regional formatting for any large numbers in the SQL summary
    python3 -c '
import locale, re, sys
locale.setlocale(locale.LC_ALL, "")
with open(sys.argv[1]) as f: content = f.read()
def fmt_num(m):
    n = int(m.group(0))
    try:
        return locale.format_string("%d", n, grouping=True)
    except Exception:
        return "{:,}".format(n)
content = re.sub(r"\b\d{4,}\b", fmt_num, content)
with open(sys.argv[1], "w") as f: f.write(content)
' "$SQL_SUMMARY"

    # Expand the $sql_now / $sql_os that were written literally (because of quoted heredoc) so the //Date and **Date** have real values (InMemory summary has the same literal-$ quirk today)
    sed -i "s/\$sql_now/$sql_now/g; s/\$sql_os/$sql_os/g" "$SQL_SUMMARY"

echo "SQL roundtrip summary written to: $SQL_SUMMARY"

# Log a clear "finished" marker (with timestamp) in all three summary files.
# This lets us easily tell (even if the runner log is truncated or we are
# looking at the .md files later) that the full run completed successfully.
finish_time=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
for f in "$SUMMARY_FILE" "$INMEM_SUMMARY" "$SQL_SUMMARY"; do
    if [[ -f "$f" ]]; then
        echo "" >> "$f"
        echo "$finish_time ---Finished---" >> "$f"
    fi
done

# Also ensure the marker is present in any pre-existing summary files
# that might have been left from a previous interrupted run in this tree
# (so the next promote will carry the "finished" state).
# For old files we just stamp with the current time (we don't know the
# original completion time).
stamp_time=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
for base in test-results test-summary; do
    for f in $(find "$base" -name "TS_STORE_*_Summary.md" 2>/dev/null); do
        if [[ -f "$f" ]] && ! grep -q " ---Finished---$" "$f" 2>/dev/null; then
            echo "" >> "$f"
            echo "$stamp_time ---Finished---" >> "$f"
        fi
    done
done

# Promote the lightweight summaries out to the sibling test-summary/ tree.
# These small .md files are safe (and intended) to commit for proof/archival.
# The massive per-run logs, .jtext, .bin etc. remain in the ignored test-results/<disk>/ tree.
"$SCRIPT_DIR/promote_summaries.sh" --all || true