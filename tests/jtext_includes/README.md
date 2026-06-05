# ts_store jText Includes (common across tests)

There are **3 common Schema files** (.jschma) and **3 common Field List files** (.jtFlds).

Per-test, only the actual **Data files** (.jtext for main + _Ints + _Floats) are generated. These reference the common schemas and field lists via jText include markers so definitions are not duplicated.

## Common files (committed in this directory)
### Schemas (3)
- ts_store_main_schema.jschma
- ts_store_ints_schema.jschma
- ts_store_floats_schema.jschma

### Field Lists (3)
- ts_store_main_fields.jtFlds
- ts_store_ints_fields.jtFlds
- ts_store_floats_fields.jtFlds

## Usage in per-test Data .jtext files
Each test's three data files (e.g. persist.jtext, persist_Ints.jtext, persist_Floats.jtext) contain markers like:

```
=== <#include#> Schema: ../../../tests/jtext_includes/ts_store_main_schema.jschma ===
=== <#include#> Fields: ../../../tests/jtext_includes/ts_store_main_fields.jtFlds ===
```

(The relative path is from `test_results/jText_logs/<TEST_NAME>/` back to the tests root.)

This way the structure (schema + fields) is shared and maintained in one place, while only the actual recorded data rows are per-test (and not committed).

The data rows use compact format with `|` as field separator (per jText SPEC §2.6.3). Null values (when present) use embedded ASCII Unit Separator (0x1F) as `|\x1F|` (to distinguish from empty `||`).

See the generator in `include/beman/ts_store/.../JTextSplitEventLog.cpp` (write_all_headers).
