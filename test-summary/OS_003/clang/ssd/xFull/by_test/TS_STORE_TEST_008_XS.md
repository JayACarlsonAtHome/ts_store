# TS_STORE_TEST_008_XS

**Purpose:** Flag routing at xFull scale (1M events/run × 3 runs, persist on final run): `KeeperRecord` → jText, `DatabaseEntry` → SQLite on disjoint indices — same verification as 008 TS.

| Compiler | Persist | Output | Records | Duration | Status | Log |
|----------|---------|--------|---------|----------|--------|-----|
| clang | flags | off | 1000000 | 1.19s | PASS | [log](../flags_logs/TS_STORE_TEST_008_XS/clang_flags_off.log) |

