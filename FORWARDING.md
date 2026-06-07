# FORWARDING / HANDOFF — ts_store (2026-06-07)

**Latest pushed commit:** see `git log -1` on `dev-work`.

**Context:** Test orchestration is C++ `ts_test_cli` + C++23 modules for reporting/runner. SQL persist works across 001–007. Legacy `TS_STORE_*_Summary.md` retired.

---

## Resume quickly (if the agent console froze)

Long matrix runs or full dual-compiler rebuilds can stall the IDE agent. **Run heavy work yourself in a normal terminal** and point the next agent at the log/output.

```bash
cd /slowdata/git/ts_store

# Fast sanity (already-built tree, ~10s)
cd build-dual/gcc && scl enable gcc-toolset-15 -- ./ts_test_cli run --compiler gcc --disk ssd

# Full dual-compiler smoke (~1–2 min total — run SEQUENTIALLY, not in parallel)
cd build-dual/gcc  && scl enable gcc-toolset-15 -- ./ts_test_cli run --compiler gcc --disk ssd
cd build-dual/clang && ./ts_test_cli run --compiler clang --disk ssd

# Rebuild after CMake/module changes (Ninja required)
./scripts/build_dual_compilers.sh          # clean rebuild both compilers (~few min)
# or incremental:
cd build-dual/gcc && scl enable gcc-toolset-15 -- cmake --build . --target ts_test_cli -j
```

**Build dirs on disk:** `build-dual/gcc`, `build-dual/clang` (Debug, jtext+sqlite ON). Modules need **Ninja** (`scripts/build_dual_compilers.sh`: `$NINJA`, PATH, or CLion-bundled fallback).

**After a good run:** `./scripts/promote_summaries.sh --all` to refresh committed `test-summary/`.

**Parallel gcc+clang runs** on the same leaf can cause SQLite `disk I/O error` during summarize — always run compilers sequentially.

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
| `tests/test_params.txt` | `SIZE`, `DISK_TYPE`, `OS_ID`, `001..007=x`, `flags=x` |

### Modules (C++23)

CMake target `jac_jtext` uses `FILE_SET cxx_modules`. Dependency chain:

```
ts_test_cli
  └─ jac.test_framework
       └─ jac.report
            ├─ jac.jtext.reader  ──► jac.jtext.core
            ├─ jac.jtext.writer  ──► jac.jtext.core
            ├─ jac.jtext         (umbrella: re-exports core + reader + writer)
            └─ jac.qlite         (shim over ../jacQlite)
```

| Module | Files | Exports |
|--------|-------|---------|
| `jac.jtext.core` | `jac.jtext.core.cppm` | `CaseMode`, `JTextEntry`, `JTextSection` |
| `jac.jtext.reader` | `jac.jtext.reader.cppm` | `JTextFile` (+ core) |
| `jac.jtext.writer` | `jac.jtext.writer.cppm` | `JTextWriter`, `write_file_comment_header` (+ core) |
| `jac.jtext` | `jac.jtext.cppm` | Umbrella re-export of all three |
| `jac.qlite` | `jac.qlite.cppm` | `jac::qlite::Sqlite`, `SqliteError` |
| `jac.report` | `jac.report.cppm` + `manifest.cpp`, `summarize.cpp` | `import jac.jtext.reader;` + `import jac.qlite;` |
| `jac.test_framework` | `jac.test_framework.cppm` + `runner.cpp` | `export import jac.report` |

**ts_store modules (incremental):**

| Module | Exports | CMake target |
|--------|---------|--------------|
| `jac.ts_store.config` | `bounded_string`, `ts_store_config` | `jac_ts_store_config` |
| `jac.ts_store.ansi` | `ansi::colors_enabled`, color helpers | `jac_ts_store_ansi` |
| `jac.ts_store.flags` | `TsStoreFlags`, `set_user_flag`, etc. | `jac_ts_store_flags` |
| `jac.ts_store.test_options` | `parse_test_options`, `TestOptions` | `jac_ts_store_test_options` |
| `jac.ts_store.persistence.jtext` | `JTextSplitEventLog`, `JTextEventSink`, `IEventSink` | `jac_ts_store_persistence_jtext` |
| `jac.ts_store.persistence.binary` | `BinaryEventLog`, `BinaryEventSink` | `jac_ts_store_persistence_binary` |
| `jac.ts_store.persistence.sql` | `SqlEventSink`, `IEventSink` | `jac_ts_store_persistence_sql` |
| `jac.ts_store.persistence.writer` | `DoubleBufferedWriter`, `IEventSink` | `jac_ts_store_persistence_writer` |
| `jac.ts_store.persistence.common` | `PersistMode`, `PersistedEvent`, `IEventSink` | `jac_ts_store_persistence_common` |
| `jac.ts_store.core` | `ts_store<Config>` (+ impl_details members) | `jac_ts_store_core` |
| `jac.ts_store.impl.testing` | umbrella: `core` + `test_options` + `memory_guard` | `jac_ts_store_impl_testing` |

`jac.ts_store.core` `export import`s `config`, `flags`, `ansi`, `writer`. Stress tests link `jac_ts_store_impl_testing` and consume modules via `import` (001–007 TS/XS). Persistence sink modules (`binary`, `jtext`, `sql`, `writer`) `export import jac.ts_store.persistence.common`. `ts_store_flags` uses `import` (flags + test_options).

**Not modularized yet:** jText internals (`jtext_core` still a static lib; `jac.jtext.*` are shims). `tools/jtext_cli/` still uses headers. Implementation bodies for ts_store remain under `include/beman/ts_store/ts_store_headers/` — module `.cppm` files are thin re-export shims over those headers.

**Module roadmap status:**

| Step | Item | Status |
|------|------|--------|
| 1 | Unified `test_framework` (dedupe includes) | **Done** |
| 2 | `jac.qlite` module | **Done** |
| 3 | jText module(s) | **Done** — partitioned `core` / `reader` / `writer` + umbrella (shims) |
| 4 | Reporting extracted from CLI | **Done** — `jac.report` |
| 5 | ts_store module boundaries + consumer `import` | **Done** — config, flags, ansi, persistence, core; tests/examples/cli |
| 6 | Move implementation bodies into module TUs | **Not started** — break up `ts_store.hpp`, modularize `impl_details/` |

### Build
```bash
./scripts/build_dual_compilers.sh   # build-dual/{gcc,clang}, jtext+sqlite ON, Ninja
```
- **GCC 15+** via `scl enable gcc-toolset-15` (tested: GCC 15.2.1)
- **Clang 21+** system `clang++` (tested: Clang 21.1.8) — CMake and `build_dual_compilers.sh` reject older versions
- jText: **reference** (`../jText`) in dev; **vendored** via `./scripts/Sync_dependencies.sh --sync jText`
- jacQLite: **reference** (`../jacQlite`) — required for `ts_test_cli`; `TS_STORE_JACQLITE_MODE=vendored` is wired in CMake but `vendor/jacQlite/` + sync script do not exist yet
- `ts_test_cli` / modules require **both** `TS_STORE_ENABLE_JTEXT_PERSIST=ON` and `TS_STORE_ENABLE_SQLITE_PERSIST=ON`

### Results layout
```
test-results/OS_00n/<disk>/<Smoke|xFull>/
  run_manifest.jtext      # matrix manifest (jText, Case: Sensitive)
  run_manifest.db         # SQLite views (gitignored)
  README.md               # leaf hub → by_test/
  by_test/<TEST>.md       # per-binary detail
  binary_logs|jText_logs|sql_logs|inmem_logs|unit_logs/TS_STORE_TEST_*_{compiler}_{persist}_{on|off}.log
```

### Promoted / committed proof (`test-summary/`)
```
test-summary/README.md
test-summary/OS_003/ssd/Smoke/README.md
test-summary/OS_003/x7k/Smoke/README.md
```
- `.gitignore`: `test-results/` ignored; `!test-summary/**/run_manifest.jtext` whitelisted; `!modules/jac.jtext/**` (dir name matches `*.jtext`); C++23 module objects (`*.gcm`, `*.pcm`, `*.ifc`, `*.ddi`, `*.modmap`, `**/CXX.dd`, plus `*.o`/`*.a`) — sources under `modules/` only, never BMIs

### `ts_test_cli` commands
```bash
./ts_test_cli run [--compiler gcc|clang|all] --disk x7k|10k|ssd [--params file]
./ts_test_cli summarize [--disk ...]
./ts_test_cli summarize-hub
./ts_test_cli promote [--all]
```

---

## What was completed

### Module stack (through jac.jtext partition)
- `jac.report`, `jac.test_framework`, `jac.qlite` extracted
- `jac.jtext` phase 1: monolithic shim
- `jac.jtext` phase 2: split into `core` / `reader` / `writer` + umbrella
- `jac.report` uses `import jac.jtext.reader` (not raw `#include <jText.h>`)

### Verified smoke (2026-06-07, post-import migration)
| Leaf | Scenarios | Status |
|------|-----------|--------|
| `OS_003/ssd/Smoke` | 226/226 (113 gcc + 113 clang) | PASS |
| `OS_003/x7k/Smoke` | 224/224 | PASS (prior run; re-run to pick up flags) |

Hub: [test-summary/README.md](test-summary/README.md)

---

## How to run on a new machine

```bash
# 1. Toolchain (RHEL 9-like): GCC 15 + Clang 21
scl enable gcc-toolset-15 -- bash   # GCC 15.2.1+
# sqlite-devel, clang++ 21+, ninja (or CLion-bundled / $NINJA)

# 2. Siblings (reference mode)
# ../jText, ../jacQlite next to ts_store

# 3. Build
./scripts/build_dual_compilers.sh

# 4. Smoke (SEQUENTIAL — one compiler at a time)
cd build-dual/gcc  && ./ts_test_cli run --compiler gcc --disk ssd
cd build-dual/clang && ./ts_test_cli run --compiler clang --disk ssd
./scripts/promote_summaries.sh --all

# 5. Before push (if sibling jText changed)
./scripts/Sync_dependencies.sh --update-checksums jText
./scripts/Sync_dependencies.sh --sync jText
```

**Params** (`tests/test_params.txt`): `SIZE=smoke`, `DISK_TYPE=ssd`, `OS_ID=OS_003`, all `001..007=x`, `flags=x`.

---

## Open / next (prioritized)

### Near-term (optional)
1. **Re-run `OS_003/x7k/Smoke`** — current leaf is 224/224 (predates `flags=x`); expect 226/226 after re-run + promote
2. **Move ts_store implementation into module TUs** — bodies still in `include/beman/...`; modules are shims (step 6 above)
3. **Re-run on other OS slots** — `OS_001` / `OS_002` leaves empty since legacy retirement
4. **jacQlite vendoring** — mirror jText (`vendor/jacQlite`, tracked list, sync script)

### Nice-to-have
- CI workflow (build + ssd smoke gate)
- Metric table CREATE formatting in `SqlEventSink` debug `.sql`
- `xFull` matrix run + promote when ready for stress evidence
- Upstream jText partition in `../jText` (ts_store shims can stay as thin re-exports)
- Manifest file locking (parallel gcc+clang on same leaf races SQLite today)

---

## Quick pointers

| Topic | Path |
|-------|------|
| CLI entry | `tools/test_cli/main.cpp` |
| Runner logic | `modules/jac.test_framework/runner.cpp` |
| Manifest write/merge | `modules/jac.report/manifest.cpp` |
| Summarize + hub | `modules/jac.report/summarize.cpp` |
| jText modules | `modules/jac.jtext/jac.jtext.{core,reader,writer}.cppm` |
| ts_store config module | `modules/jac.ts_store/jac.ts_store.config.cppm` |
| ts_store ansi module | `modules/jac.ts_store/jac.ts_store.ansi.cppm` |
| ts_store flags module | `modules/jac.ts_store/jac.ts_store.flags.cppm` |
| ts_store sql persistence module | `modules/jac.ts_store/jac.ts_store.persistence.sql.cppm` |
| ts_store writer module | `modules/jac.ts_store/jac.ts_store.persistence.writer.cppm` |
| ts_store persistence common | `modules/jac.ts_store/jac.ts_store.persistence.common.cppm` |
| ts_store core module | `modules/jac.ts_store/jac.ts_store.core.cppm` |
| ts_store impl.testing module | `modules/jac.ts_store/jac.ts_store.impl.testing.cppm` |
| jacQLite module shim | `modules/jac.qlite/jac.qlite.cppm` |
| Sqlite forwarder | `include/.../persistence/Sqlite.hpp` → `../jacQlite` |
| Promote script | `scripts/promote_summaries.sh` |
| jText sibling | `../jText` / `vendor/jText` |
| jacQLite sibling | `../jacQlite` |
| Dual-compiler docs | `DUAL_COMPILER_BUILD.md` |

---

## Pitfalls for the next agent

1. **Agent console freezes** — avoid blocking on `build_dual_compilers.sh` or full matrix in-agent; use incremental builds and single-compiler smoke, or tell user to run in external terminal
2. **Parallel gcc+clang** on same leaf → SQLite `disk I/O error` during summarize; run sequentially
3. **`[13/113]` in logs** — scenario progress index, not a compiler version
4. **Ninja required** for C++ modules; plain Make generator fails
5. **Minimal cmake** (jtext/sqlite OFF) breaks `ts_test_cli` — dual build always enables both
6. **Compiler-specific binaries** — run gcc from `build-dual/gcc`, clang from `build-dual/clang`
7. **Build uses `../jText`** unless `TS_STORE_JTEXT_MODE=vendored`
8. **Manifest merge** — gcc then clang on same leaf → `compilers_csv: gcc,clang`, 226 scenarios (113 per compiler incl. `ts_store_flags`)
9. **Do not resurrect** `tools/test_cli/manifest.cpp` / `summarize.cpp` — in `modules/jac.report/`
10. **Do not regenerate** old `TS_STORE_*_Summary.md`

---

Testing framework + module reporting stack: **functionally complete** for smoke.

**Module status:** consumer migration **complete** (stress tests 001–007 TS/XS, flags, examples, `ts_test_cli` use `import`). Implementation migration **not started** (canonical code still in `include/beman/ts_store/ts_store_headers/`).

— session handoff 2026-06-07 (doc sync: consumer vs implementation migration)