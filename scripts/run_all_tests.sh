#!/bin/bash
#
# run_all_tests.sh
#
# Automation to run all ts_store stress tests (001-007 TS/XS + flags).
# Now exercises double-buffered persistence (async) for *every* test, once with
# BinaryEventSink and once with JTextEventSink (via --persist).
#
# Output logs + persist artifacts go under:
#   test_results/binary_logs/TS_STORE_TEST_001_TS/
#   test_results/jText_logs/TS_STORE_TEST_001_TS/
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
# test_results/ tree. README.md links to the summary.
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

# test_results/ layout (define early so cleanup and everything can reference them)
TEST_RESULTS_BASE="$PROJECT_ROOT/test_results"
BINARY_LOGS="$TEST_RESULTS_BASE/binary_logs"
JTEXT_LOGS="$TEST_RESULTS_BASE/jText_logs"
mkdir -p "$BINARY_LOGS"
mkdir -p "$JTEXT_LOGS"

# === Cleanup of legacy/unused stuff (part of the runner now) ===
# Remove old results/ dir (legacy)
if [ -d "$PROJECT_ROOT/results" ]; then
  echo "Removing legacy results/ directory..."
  rm -rf "$PROJECT_ROOT/results"
fi

# Remove historical unused build directories that clutter the tree
for d in build-clean-check build-double-test build-dual build-no-persist; do
  if [ -d "$PROJECT_ROOT/$d" ]; then
    echo "Removing unused build directory: $d"
    rm -rf "$PROJECT_ROOT/$d"
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

# Helper to prepare the log dir for a given test + persist type + output mode.
# Sets globals: logdir_name, sdir, ldir
prepare_log_dir() {
    local tname="$1"
    local pval="$2"
    if [[ "$pval" == "jtext" ]]; then
        logdir_name="jText_logs"
    else
        logdir_name="binary_logs"
    fi
    sdir=$(test_to_subdir_name "$tname")
    ldir="$TEST_RESULTS_BASE/$logdir_name/$sdir"
    mkdir -p "$ldir"
}

# Generate the rich markdown summary document.
# Scans test_results/... for .meta + .log files (supports multiple compilers).
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

    local suite_dur_display="${suite_duration_sec}s"
    if [[ "$suite_duration_sec" =~ ^[0-9]+$ ]] && (( suite_duration_sec > 60 )); then
        suite_dur_display="$((suite_duration_sec / 60))m $((suite_duration_sec % 60))s (${suite_duration_sec}s)"
    fi

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
        ["TS_STORE_TEST_002_TS"]=2500
        ["TS_STORE_TEST_002_XS"]=2500
        ["TS_STORE_TEST_003_TS"]=20000
        ["TS_STORE_TEST_003_XS"]=20000
        ["TS_STORE_TEST_004_TS"]=100000
        ["TS_STORE_TEST_004_XS"]=100000
        ["TS_STORE_TEST_005_TS"]=1000000
        ["TS_STORE_TEST_005_XS"]=1000000
        ["TS_STORE_TEST_006_TS"]=20000
        ["TS_STORE_TEST_006_XS"]=20000
        ["TS_STORE_TEST_007_TS"]=1000000
        ["TS_STORE_TEST_007_XS"]=1000000
        ["TS_STORE_TEST_flags"]="?"
    )
    declare -A KNOWN_PERSIST=( ["binary_logs"]="binary" ["jText_logs"]="jtext" )

    for logtype_dir in "$results_base/binary_logs" "$results_base/jText_logs"; do
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
                for f in "$logtype_dir/$SUBDIR"/persist*; do
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

                local rel_path="test_results/$(basename "$logtype_dir")/$SUBDIR/${COMPILER}.log"
                local log_link="[log]($rel_path)"

                rowkey="${COMPILER}|${SUBDIR}|${LOGTYPE}|${TEST_OUTPUT_MODE}"
                if [[ -z "${seen[$rowkey]:-}" ]]; then
                    seen[$rowkey]=1
                    rows+=("| ${COMPILER} | ${SUBDIR} | ${LOGTYPE} | ${TEST_OUTPUT_MODE} | ${RECORDS:-?} | ${DURATION_SEC}s | ${persist_human} | ${persist_mbs} | ${STATUS} | ${log_link} |")
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
                else
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
        for pdir in binary_logs jText_logs; do
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
                    local rlog="test_results/$pdir/$tname/${COMPILER}_${ltype}_${om}.log"
                    rows+=("| ${COMPILER} | ${tname} | ${ltype} | ${om} | ${rec} | ${dur} | ${phuman} | ${pmbs} | ? | [log]($rlog) |")

                    # populate scenario_data for the faster section too
                    skey="${tname}|${COMPILER}|${om}"
                    cur="${scenario_data[$skey]:-}"
                    read -r cb_dur cb_size cb_rec cj_dur cj_size cj_rec <<< "$cur" 2>/dev/null || { cb_dur=""; cb_size=""; cb_rec=""; cj_dur=""; cj_size=""; cj_rec=""; }
                    if [[ "$ltype" == "binary" ]]; then
                        cb_dur="$dur"
                        cb_size="$pbytes"
                        cb_rec="$rec"
                    else
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
        echo "(No comparative data yet — run for both gcc and clang.)" >> "$out_file"
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
        printf "%s\n" "${keyed_faster[@]}" | sort -t'|' -k1,1 -k2,2 -k3,3 | cut -d'|' -f4- >> "$out_file"
    fi

    # Notes + Configs
    cat >> "$out_file" <<'NOTES'

## Notes
- All tests now attach a `DoubleBufferedWriter` + chosen sink (`BinaryEventSink` or `JTextEventSink`) for asynchronous background persistence. Hot path remains fast.
- Persist artifacts (`.bin` or the 3 `.jtext` files) are written using the `--base-name` passed by the runner so they land inside the corresponding `test_results/*/TS_STORE_TEST_.../` subdirectory.
- `Records` are best-effort parsed from test output (for 005/007 this is typically 1,000,000 × 50 = 50M per invocation).
- Size and Rate columns: measured on-disk persist artifact size and effective MB/s (size / full test duration). The "Log" column links to the captured stdout for that run.
- Each (test, logtype) is executed twice: once with output=on (live console, ANSI colors enabled via --color=1) and once with output=off (silent capture, --color=0). This shows the overhead of console output (many events set LogConsole). Live "on" runs are colorful and pretty; saved logs are always plain text (ANSI stripped).
- Compile/build time is for the full set of test binaries for that compiler (Release + jText enabled).
- Default (no --compiler or --compiler all) runs gcc then clang internally in one invocation. Use --compiler gcc|clang to run just one. The summary table and faster comparisons will include all compilers' data.
- Legacy `results/<compiler>/` tree may still exist from prior versions; new canonical location is `test_results/`.

## Config settings used by the tests (LogConfig)
- 001/002/003/006/007 (TS): `ts_store_config<true, 6, 20, 43, 9, 6, false>`
- 001/002/003/006/007 (XS): `ts_store_config<false, 6, 20, 43, 9, 6, false>`
- 004 (TS): `ts_store_config<true, 6, 20, 75, 9, 6, false>` (main) + result config
- 004 (XS): `ts_store_config<false, 6, 20, 75, 9, 6, false>`
- 005 (TS): `ts_store_config<true, 6, 20, 43, 9, 6, false>` (250 threads × 4000 × 50 runs)
- 005 (XS): `ts_store_config<false, 6, 20, 43, 9, 6, false>`
- flags: standalone `TsStoreFlags` unit test (no store / no persist)

See main [README.md](README.md) and the test sources under `tests/` for details.

NOTES

    echo "Summary updated: $out_file"
}

COMPILER="all"
OUTPUT_MODE="yes"  # yes = live console (with ANSI colors for test output), no = silent (logs only)

usage() {
    echo "Usage: $0 [--compiler gcc|clang|all] [--output yes|no]"
    echo "  Default: all (runs gcc then clang internally, one summary at end)"
    echo "  --output yes : live console output with ANSI colors (pretty)"
    echo "  --output no  : silent (logs only)"
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
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

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

echo "=== ts_store All Stress Tests Runner (double-buffered persist for ALL tests) ==="
echo "Compilers: ${COMPILER_LIST[*]}"
echo "Output mode: $OUTPUT_MODE (live console + ANSI colors when 'yes')"
echo "Primary output: test_results/binary_logs/ + jText_logs/ + TS_STORE_Test_Summary.md"
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
  echo "Cleaning previous $COMPILER artifacts from test_results/..."
  for logdir in "$BINARY_LOGS" "$JTEXT_LOGS"; do
    for sub in "$logdir"/TS_STORE_TEST_* ; do
      [ -d "$sub" ] || continue
      rm -f "$sub/${COMPILER}_"*.log "$sub/${COMPILER}_"*.meta 2>/dev/null || true
      # Also remove previous persist artifacts so re-runs always produce files with current
      # standardized //File Name / //Date / //Purpose headers (for jtext/sql/bin + field lists).
      rm -f "$sub"/{persist,persist_Ints,persist_Floats}.{bin,jtext,sql} 2>/dev/null || true
    done
  done

  # Determine build dir and how to invoke cmake/build
BUILD_DIR="$PROJECT_ROOT/build-results-$COMPILER"
CMAKE_CMD="cmake"
BUILD_CMD="cmake --build . --target"

if [[ "$COMPILER" == "gcc" ]]; then
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON"
    # Enable jText for the big double-buffer tests (005/007) so they can use
    # JTextSplitEventLog via JTextEventSink, producing separate _Ints.jtext and
    # _Floats.jtext files with all the metric data (in addition to the main log).
    # The stress tests (001-007) are always built.
else
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DTS_STORE_ENABLE_JTEXT_PERSIST=ON"
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
for src in "$PROJECT_ROOT/tests/ts_store_001/test_001_TS.cpp" \
           "$PROJECT_ROOT/CMakeLists.txt" \
           "$PROJECT_ROOT/include/beman/ts_store/ts_store_headers/impl_details/test_options.hpp" ; do
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

# === New structured run: every test x2 (binary + jtext), logs + persist artifacts under test_results/ ===

# Combinatorial matrix (the fields whose product gives the scenario count):
# 1. Compiler dimension (from COMPILER_LIST): "gcc", "clang"  (1 or 2; "all" runs both sequentially)
# 2. Test dimension (this list): 15 entries
#    - 001_TS / 001_XS ... 007_TS / 007_XS   (timestamps on vs off)
#    - flags
# 3. Persist/log type: "binary", "jtext"  (2)
# 4. Output mode: "on" (live console + ANSI colors via --color=1), "off" (silent redirect, --color=0)  (2)
#    Table sorted by Test, LogType, Output (on before off), Compiler (clang before gcc) for easy side-by-side comparison.
#
# Per-compiler scenarios: 15 × 2 × 2 = 60
# When running "all" (the default): 2 compilers × 60 = 120 total scenarios
#
# What each scenario produces:
# - Runner capture: ${COMPILER}_${logtype}_${mode}.log   (stdout of the test binary)
# - Meta data:       ${COMPILER}_${logtype}_${mode}.meta (duration, records, persist size, status, etc.)
# - Persist artifacts: the actual on-disk logs written by the test
#     (persist.bin  or  persist.jtext + persist_Ints.jtext + persist_Floats.jtext)
#     placed in the TS_STORE_TEST_... subdir via --base-name
#
# Subdirectory layout (one per test dimension value):
#   test_results/binary_logs/TS_STORE_TEST_00N_XX/   (and same under jText_logs/)
declare -a TESTS=(
    "ts_store_001_TS"
    "ts_store_001_XS"
    "ts_store_002_TS"
    "ts_store_002_XS"
    "ts_store_003_TS"
    "ts_store_003_XS"
    "ts_store_004_TS"
    "ts_store_004_XS"
    "ts_store_005_TS"
    "ts_store_005_XS"
    "ts_store_006_TS"
    "ts_store_006_XS"
    "ts_store_007_TS"
    "ts_store_007_XS"
    "ts_store_flags"
)

echo
echo "=== Running all tests (double-buffered async persist for every test) ==="
echo "Binary logs : $BINARY_LOGS/"
echo "jText logs  : $JTEXT_LOGS/"
echo "Combinatorics: ${#TESTS[@]} tests × 2 logtypes × 2 output modes = 60 per compiler (120 when both)"
echo "Each (test, logtype) is run twice per compiler: once with output=on (live console + ANSI colors), once with output=off (silent, no color)."
echo

TOTAL_SCENARIOS=$(( ${#TESTS[@]} * 2 * 2 ))  # 15 tests × 2 logtypes × 2 output modes = 60 per compiler
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

    for persist_val in binary jtext; do
        logtype="$persist_val"
        prepare_log_dir "$test" "$persist_val"
        persist_base="$ldir/persist"

        for test_output_mode in on off; do
            run_log="$ldir/${COMPILER}_${logtype}_${test_output_mode}.log"

            echo "Running $test (logtype=$logtype, output=$test_output_mode) ... -> $run_log"

            start_ep=$(date +%s)

            if [[ "$test_output_mode" == "on" ]]; then
                # "on" = live console with ANSI colors enabled (pretty output visible)
                "$bin" --interactive=0 --color=1 --persist="$logtype" --base-name="$persist_base" < /dev/null 2>&1 \
                    | tee >(strip_ansi > "$run_log")
                bin_status=${PIPESTATUS[0]}
            else
                # "off" = silent, force no color (clean for logs; we strip anyway)
                "$bin" --interactive=0 --color=0 --persist="$logtype" --base-name="$persist_base" < /dev/null 2>&1 \
                    | strip_ansi > "$run_log"
                bin_status=${PIPESTATUS[0]}
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
  if [[ "$bdur" == "skipped" ]]; then
    PER_COMPILER_TIMES+="  $c: build skipped, tests: ${tdur}s"$'\n'
  else
    PER_COMPILER_TIMES+="  $c: build ${bdur}s + tests ${tdur}s"$'\n'
  fi
done

# Quick verification that the number of produced artifacts matches the combinatorics
META_COUNT=$(find "$TEST_RESULTS_BASE" -name '*.meta' | wc -l)
echo "Verification: expected $TOTAL_SCENARIOS scenarios, found $META_COUNT .meta files"

# Capture total wall time for the entire suite (including all compilers, builds, and runs)
SUITE_END_EP=$(date +%s)
SUITE_DURATION_SEC=$((SUITE_END_EP - SUITE_START_EP))

# Generate (or update) the rich combined summary document (table will contain data from all compilers)
SUMMARY_FILE="$PROJECT_ROOT/TS_STORE_Test_Summary.md"
generate_test_summary "$SUMMARY_FILE" "$COMPILER" "$COMPILER_DISPLAY" "$BUILD_INFO" "$TEST_RESULTS_BASE" "$RUN_PASSED" "$RUN_FAILED" "$TOTAL_SCENARIOS" "$SUITE_DURATION_SEC" "$PER_COMPILER_TIMES"

echo "Rich summary written to: $SUMMARY_FILE"
echo
echo "Done."