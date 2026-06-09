# FORWARDING: ts_store Sequential Checklist Build (2026-06-08)

## Current state

`ts_store` is a C++23 module-heavy event store with jText, Binary, and SQLite persistence. The **primary build workflow** is checklist-driven: mark the rows you want on this host with `[x]`, then run `./scripts/Build` once — it walks [FileCheckList.txt](FileCheckList.txt) top to bottom and runs every `[x]` row (skipping `[ ]`).

### Proven on Linux Mint 22.0 (OS_003 / ssd)

| Checklist row | Compiler | Smoke (113 scenarios) | Notes |
|---------------|----------|----------------------|-------|
| GCC / Linux Mint 22.0 / ssd | g++-15 (PPA) | PASS | Full module matrix |
| Clang / Linux Mint 22.0 / ssd | clang++-20 | PASS | `TS_STORE_CLANG_LTO=OFF` default (compile-only LTO broke links) |

Promoted summaries: `test-summary/OS_003/ssd/Smoke/` (226 manifest entries = gcc + clang).

### Other hosts (leave `[ ]` until on that machine)

On a given host, mark only the rows for **that** OS/compiler/disk with `[x]`; leave everything else `[ ]`. Example rows still to prove on target hardware:

```
[ ] GCC    / RHEL 9.8        / ssd
[ ] Clang  / RHEL 9.8        / ssd
[ ] GCC    / RHEL 10.2       / x7k
[ ] Clang  / RHEL 10.2       / x7K
```

The script **never edits** `FileCheckList.txt` — `[x]` / `[ ]` are user-selected only.

### Legacy / bridge

- `./scripts/build_dual_compilers.sh` — still works on RHEL; **not** the primary path. See [DUAL_COMPILER_BUILD.md](DUAL_COMPILER_BUILD.md).
- `build-dual/`, `build-mint/`, `build-local/` — ad-hoc trees from earlier work; gitignored. Sequential builds use transient `build-seq/` (removed after each successful row).

---

## Primary command

```bash
./scripts/Build FileCheckList.txt --FullRebuild=On --SmokeTest=On --FullTest=Off
```

**Flags**

| Flag | Effect |
|------|--------|
| `--FullRebuild=On` | `rm -rf build-seq/<platform>-<compiler>` before configure |
| `--SmokeTest=On` | Run matrix with `SIZE=smoke` |
| `--FullTest=On` | Run matrix with `SIZE=full` (xFull leaf) |
| Both Off | Build only; no `ts_test_cli run` |

**Checklist markers (user-edited only)**

| Marker | Meaning |
|--------|---------|
| `[x]` | Run this row when `./scripts/Build` is invoked |
| `[ ]` | Skip (wrong host, not ready, run later) |

The script reads top to bottom and **does not change** any marker after a run.

**Per `[x]` row (automatic)**

1. Read the row from [FileCheckList.txt](FileCheckList.txt) (in file order)
2. Resolve compiler (Mint GCC → `g++-15`; Mint Clang → `clang++-20`…`18`; RHEL GCC → `gcc-toolset-15` via `scl`)
3. `source scripts/build_common.sh` → Ninja ≥ 1.11
4. Configure + build full matrix in `build-seq/<slug>/`
5. Run `ts_test_cli` with generated `run_params.txt` (disk from row; SIZE from flags; other defaults from [tests/test_params.txt](tests/test_params.txt))
6. `./scripts/promote_summaries.sh --disk <disk>`
7. Delete `build-seq/<slug>/`; continue to the next line

**Adding a row** — append to `FileCheckList.txt` (usually `[ ]` until you are on the target host):

```
[ ] GCC    / Linux Mint 22.0 / 10k
```

Use `GCC` or `Clang`, platform string (free text), disk (`ssd`, `x7k`, `10k`). Mark `[x]` on the rows you want before invoking `./scripts/Build`.

---

## Key files

| File | Role |
|------|------|
| [FileCheckList.txt](FileCheckList.txt) | User-editable run selector (`[x]` = run, `[ ]` = skip) |
| [scripts/Build](scripts/Build) | Sequential orchestrator |
| [scripts/build_common.sh](scripts/build_common.sh) | Ninja resolution |
| [scripts/promote_summaries.sh](scripts/promote_summaries.sh) | `test-results/` → `test-summary/` |
| [tests/test_params.txt](tests/test_params.txt) | Default SIZE, DISK_TYPE, test selection |
| [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md) | Structure, modules, test matrix, repo layout |
| [Doc/linux_mint_gcc15.md](Doc/linux_mint_gcc15.md) | Mint toolchain (GCC 15 PPA, Clang 20) |
| [CMakeLists.txt](CMakeLists.txt) | `TS_STORE_GNU_RELEASE_O3`, `TS_STORE_CLANG_LTO`, persist options |

---

## Compiler / CMake notes (Mint)

- **GCC 15 PPA:** `-O3` on module consumers can ICE → `TS_STORE_GNU_RELEASE_O3=OFF` (default).
- **Clang 20:** `-flto=thin` was compile-only → bitcode `.o` link failures. Fixed: `TS_STORE_CLANG_LTO=OFF` (default); enable only with matching link LTO.
- **g++-14:** not sufficient for full module matrix on Mint ([test-composer/build_report.txt](test-composer/build_report.txt)).
- **Modules are not for portability.** New machine, CPU, OS, or compiler → **full rebuild of all modules once**, then reuse that build for testing/incremental work on the same host. BMIs must never be copied between environments. `--FullRebuild=On` clears the sequential build dir. See [Doc/ARCHITECTURE.md § Modules are not for portability](Doc/ARCHITECTURE.md#modules-are-not-for-portability).

---

## Dependencies mode

`scripts/Build` uses **reference** mode when `../jText` and `../jacQlite` exist; otherwise **vendored** (`vendor/`). Same as the old dual script.

---

## Status logging

[build_status.txt](build_status.txt) — informal timestamped log from earlier iteration. Not required for `./scripts/Build`; safe to tail for history.

---

## Next steps

1. On each target host: mark that host's rows `[x]`, leave others `[ ]`, then `./scripts/Build …`.
2. Repeat for RHEL 10.2 / other disks when those machines are available.
3. Optional: additional disks (`10k`) on Mint — add checklist rows.
4. Eventually retire `build_dual_compilers.sh` once all rows are proven on their target hosts.