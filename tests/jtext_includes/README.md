# ts_store jText Includes (common across tests)

Shared schema and field-list companions live here. Per-run **data** files reference them via jText includes so definitions are not duplicated.

## jText file profiles (see `../jText/SPEC.md` §2.0)

| Profile | Used in ts_store for | Section markers | Field includes |
|---------|----------------------|-----------------|----------------|
| **Light** | `run_manifest.jtext` (test matrix summaries) | `-- SectionName --` | `# Fields: path.jtFlds` |
| **Full** | Event persistence `.jtext` (main + _Ints + _Floats) | `=== Section: … ===` | `=== <#include#> Fields: path.jtFlds ===` |

Both profiles share the same `//` filesystem wrapper and `#` metadata block at the top of each `.jtext` file.

## Common files (committed in this directory)

### Schemas (3) — full profile companions

- `ts_store_main_schema.jschma`
- `ts_store_ints_schema.jschma`
- `ts_store_floats_schema.jschma`

### Field lists (5)

- `ts_store_main_fields.jtFlds` — event log main columns
- `ts_store_ints_fields.jtFlds` — integer sidecar columns
- `ts_store_floats_fields.jtFlds` — float sidecar columns
- `run_manifest_runmeta_fields.jtFlds` — manifest `RunMeta` section (8 fields)
- `run_manifest_scenarios_fields.jtFlds` — manifest `Scenarios` section (8 fields)

## Light profile: `run_manifest.jtext`

Written by `modules/jac.report/manifest.cpp` after each test-matrix run. Example shape:

```
//File:    …/run_manifest.jtext
//Date:    YYYY-MM-DD
//Purpose: ts_store test matrix run manifest
//Related: type=ts_store table=ts_run_manifest
//
# JText File - created …
# Purpose - ts_store test matrix run manifest
# Case: Sensitive
# Table Name: ts_run_manifest

-- RunMeta --
# Fields: …/run_manifest_runmeta_fields.jtFlds
 1. #|# OS_001|ssd|Smoke|…

-- Scenarios --
# Fields: …/run_manifest_scenarios_fields.jtFlds
  1. #|# TS_STORE_TEST_001_TS|gcc|…
```

Reference: `../jText/samples/light_profile/run_manifest.jtext`

## Full profile: per-test event `.jtext` files

Each test's three data files (e.g. `persist.jtext`, `persist_Ints.jtext`, `persist_Floats.jtext`) contain:

```
=== <#include#> Schema: ../../../tests/jtext_includes/ts_store_main_schema.jschma ===
=== <#include#> Fields: ../../../tests/jtext_includes/ts_store_main_fields.jtFlds ===
```

(Relative path is from `test-results/.../jText_logs/<TEST_NAME>/` back to the repo `tests/` root.)

Data rows use compact `#|#` format (SPEC §2.6.3). Null values use ASCII Unit Separator (`|\x1F|`) to distinguish from empty `||`.

Generator: `include/beman/ts_store/.../JTextSplitEventLog.cpp` (`write_all_headers`).