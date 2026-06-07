# FORWARDING / HANDOFF — ts_store (2026-06-07)

**Latest pushed commit:** `f815183` — *Add jac.qlite module shim; depend on sibling jacQLite* on `dev-work`.

**Uncommitted work (this session):** `jac.jtext` module added; `jac.report` switched from `#include <jText.h>` to `import jac.jtext`. Smoke re-run 224/224 PASS. See [Modules](#modules-c23) below.

**Context:** Test orchestration is C++ `ts_test_cli` + C++23 modules for reporting/runner. SQL persist works across 001–007. Legacy `TS_STORE_*_Summary.md` retired.

---

## Resume quickly (if the agent console froze)

Long matrix runs or full dual-compiler rebuilds can stall the IDE agent. **Run heavy work yourself in a normal terminal** and point the next agent at the log/output.

```bash
cd /slowdata/git/ts_store

# Fast sanity (already-built tree, ~10s)
cd build-dual/gcc && scl enable gcc-toolset-15 -- ./ts_test_cli run --compiler gcc --disk ssd

# Full dual-compiler smoke (~1–2 min total, two terminals or sequential)
cd build-dual/gcc  && scl enable gcc-toolset-15 -- ./ts_test_cli run --compiler gcc --disk ssd
cd build-dual/clang && ./ts_test_cli run --compiler clang --disk ssd

# Rebuild after CMake/module changes (Ninja required)
./scripts/build_dual_compilers.sh          # clean rebuild both compilers (~few min)
# or incremental:
cd build-dual/gcc && scl enable gcc-toolset-15 -- cmake --build . --target ts_test_cli -j
```

**Build dirs on disk:** `build-dual/gcc`, `build-dual/clang` (Debug, jtext+sqlite ON). Modules need **Ninja** (`scripts/build_dual_compilers.sh` finds CLion-bundled ninja if not in PATH).

**After a good run:** `./scripts/promote_summaries.sh --all` to refresh committed `test-summary/`.

---

## Current architecture

### Runner (canonical)
| Piece | Role |
|-------|------|
| `tools/test_cli/main.cpp` → `ts_test_cli` | Thin CLI; `import jac.test_framework` |
| `modules/jac.test_framework/` | Params, scenarios, `run_scenario()` |
| `modules/jac.report/` | Manifest write/merge, SQLite summarize, hub |
| `scripts/ts-test` | Finds `build-dual/{gcc,clang}/ts_test_cli` and delegates |
| `scripts/run_all_tests.sh` | Thin wrapper: gcc + clang `ts_test_cli run`, then promote |
| `scripts/run_all_tests.py` | **Deleted** — do not resurrect |
| `tests/test_params.txt` | `SIZE`, `DISK_TYPE`, `OS_ID`, `001..007=x` |

### Modules (C++23)

CMake targets use `FILE_SET cxx_modules`. Dependency chain:

```
ts_test_cli
  └─ jac.test_framework  (modules/jac.test_framework/)
       └─ jac.report     (modules/jac.report/)
            ├─ jac.jtext   (modules/jac.jtext/)     ← NEW (uncommitted)
            └─ jac.qlite   (modules/jac.qlite/)     ← shim over ../jacQlite
```

| Module | Interface | Implementation | Notes |
|--------|-----------|----------------|-------|
| `jac.jtext` | `jac.jtext.cppm` | — | Re-exports `JTextFile`, `JTextEntry`, `JTextSection`, `JTextWriter`, `CaseMode` |
| `jac.qlite` | `jac.qlite.cppm` | — | Re-exports `jac::qlite::Sqlite`, `SqliteError` |
| `jac.report` | `jac.report.cppm` | `manifest.cpp`, `summarize.cpp` | `import jac.jtext;` + `import jac.qlite;` in impl units |
| `jac.test_framework` | `jac.test_framework.cppm` | `runner.cpp` | `export import jac.report` |

**Not modularized yet:** `ts_store` core headers, stress test binaries, jText internals (`jtext_core` still a static lib).

**Module roadmap status:**

| Step | Item | Status |
|------|------|--------|
| 1 | Unified `test_framework` (dedupe includes) | **Done** — `jac.test_framework` |
| 2 | `jac.qlite` module | **Done** |
| 3 | jText module(s) | **Phase 1 done** — `jac.jtext` shim; deeper partition (reader/writer/core) **TODO** |
| 4 | Reporting extracted from CLI | **Done** — `jac.report` |
| 5 | `ts_store` core/persistence modules | **TODO** |

### Build
```bash
./scripts/build_dual_compilers.sh   # build-dual/{gcc,clang}, jtext+sqlite ON, Ninja
```
- GCC 15 via `scl enable gcc-toolset-15`
- Clang: system `clang++`
- jText: **reference** (`../jText`) in dev; **vendored** via `./scripts/Sync_dependencies.sh --sync jText`
- jacQLite: **reference** (`../jacQlite`); `TS_STORE_JACQLITE_MODE=vendored` for vendor copy
- `ts_test_cli` / modules require **both** `TS_STORE_ENABLE_JTEXT_PERSIST=ON` and `TS_STORE_ENABLE_SQLITE_PERSIST=ON`

### Results layout
```
test-results/OS_00n/<disk>/<Smoke|xFull>/
  run_manifest.jtext      # matrix manifest (jText, Case: Sensitive)
  run_manifest.db         # SQLite views (gitignored)
  README.md               # leaf hub → by_test/
  by_test/<TEST>.md       # per-binary detail
  binary_logs|jText_logs|sql_logs|inmem_logs/TS_STORE_TEST_*_{compiler}_{persist}_{on|off}.log
```

### Promoted / committed proof (`test-summary/`)
```
test-summary/README.md
test-summary/OS_003/ssd/Smoke/README.md
test-summary/OS_003/x7k/Smoke/README.md
```
- `.gitignore`: `test-results/` ignored; `!test-summary/**/run_manifest.jtext` whitelisted

### `ts_test_cli` commands
```bash
./ts_test_cli run [--compiler gcc|clang|all] --disk x7k|10k|ssd [--params file]
./ts_test_cli summarize [--disk ...]
./ts_test_cli summarize-hub
./ts_test_cli promote [--all]
```

---

## What was completed

### Testing framework (pushed through `f815183`)
- C++ CLI replaces shell/Python orchestration
- SQL matrix coverage 001–007 TS/XS (`binary | jtext | sql | none`)
- Manifest → SQLite views → markdown (`run_manifest.jtext`, `by_test/*.md`)
- Per-OS layout `test-results/OS_00n/<disk>/<Smoke|xFull>/`
- `jac.report`, `jac.test_framework`, `jac.qlite` modules extracted

### jac.jtext module (uncommitted, verified 2026-06-07)
- Added `modules/jac.jtext/jac.jtext.cppm` + `jac_jtext` CMake target
- `jac.report/manifest.cpp` and `summarize.cpp`: `#include <jText.h>` → `import jac.jtext`
- **Compile:** GCC 15 + Clang, `ts_test_cli` — PASS
- **Smoke:** `OS_003/ssd/Smoke` — **224/224** (112 gcc + 112 clang), manifest + summarize pipeline PASS

### Prior verified smoke (still valid on x7k leaf)
| Leaf | Scenarios | Status |
|------|-----------|--------|
| `OS_003/ssd/Smoke` | 224/224 | PASS (re-verified with jac.jtext) |
| `OS_003/x7k/Smoke` | 224/224 | PASS (prior run; re-run after commit if desired) |

---

## How to run on a new machine

```bash
# 1. Toolchain (RHEL-like)
scl enable gcc-toolset-15 -- bash
# sqlite-devel, clang++, ninja (or CLion-bundled ninja)

# 2. Siblings (reference mode)
# ../jText, ../jacQlite next to ts_store

# 3. Build
./scripts/build_dual_compilers.sh

# 4. Smoke (run from each compiler's build dir)
cd build-dual/gcc  && ./ts_test_cli run --compiler gcc --disk ssd
cd build-dual/clang && ./ts_test_cli run --compiler clang --disk ssd
./scripts/promote_summaries.sh --all

# 5. Before push
./scripts/Sync_dependencies.sh --update-checksums jText
./scripts/Sync_dependencies.sh --sync jText
```

**Params** (`tests/test_params.txt`): `SIZE=smoke`, `DISK_TYPE=ssd`, `OS_ID=OS_003`, all `001..007=x`.

---

## Open / next (prioritized)

### Near-term
1. **Commit jac.jtext** — stage `modules/jac.jtext/`, `CMakeLists.txt`, `modules/jac.report/{manifest,summarize}.cpp`
2. **Re-run + promote** after commit; optionally refresh `OS_003/x7k/Smoke`
3. **Re-run on other OS slots** — `OS_001` / `OS_002` leaves empty since legacy retirement
4. **Flags test** — `ts_store_flags` not in matrix; optional add

### Modules (continuing)
1. **jText partition phase 2** — split `jac.jtext` into reader/writer/core modules (inside ts_store shim layer or upstream in `../jText`)
2. **`ts_store` core/persistence modules** — last; highest risk / largest surface

### Nice-to-have
- Metric table CREATE formatting in `SqlEventSink` debug `.sql`
- `xFull` matrix run + promote when ready for stress evidence

---

## Quick pointers

| Topic | Path |
|-------|------|
| CLI entry | `tools/test_cli/main.cpp` |
| Runner logic | `modules/jac.test_framework/runner.cpp` |
| Manifest write/merge | `modules/jac.report/manifest.cpp` |
| Summarize + hub | `modules/jac.report/summarize.cpp` |
| jText module shim | `modules/jac.jtext/jac.jtext.cppm` |
| jacQLite module shim | `modules/jac.qlite/jac.qlite.cppm` |
| Sqlite forwarder | `include/.../persistence/Sqlite.hpp` → `../jacQlite` |
| Promote script | `scripts/promote_summaries.sh` |
| jText sibling | `../jText` / `vendor/jText` |
| jacQLite sibling | `../jacQlite` |
| Dual-compiler docs | `DUAL_COMPILER_BUILD.md` |

---

## Pitfalls for the next agent

1. **Agent console freezes** — avoid blocking on `build_dual_compilers.sh` or full matrix in-agent; use incremental builds and `--compiler gcc` smoke only, or tell user to run in external terminal
2. **`[13/112]` in logs** — scenario progress index, not a compiler version
3. **Ninja required** for C++ modules; plain Make generator fails
4. **Minimal cmake** (jtext/sqlite OFF) breaks `ts_test_cli` — dual build always enables both
5. **Compiler-specific binaries** — run gcc scenarios from `build-dual/gcc`, clang from `build-dual/clang`; `--compiler all` from one dir still uses that dir's binaries
6. **Build uses `../jText`** unless `TS_STORE_JTEXT_MODE=vendored`
7. **Manifest merge** — gcc then clang on same leaf → `compilers_csv: gcc,clang`, 224 scenarios
8. **Do not resurrect** `tools/test_cli/manifest.cpp` / `summarize.cpp` — moved to `modules/jac.report/`
9. **Do not regenerate** old `TS_STORE_*_Summary.md`

---

Testing framework + module reporting stack: **functionally complete** for smoke. Module migration: **4 of 5 roadmap steps done** (core `ts_store` modules remain).

— session handoff 2026-06-07 (updated after jac.jtext)