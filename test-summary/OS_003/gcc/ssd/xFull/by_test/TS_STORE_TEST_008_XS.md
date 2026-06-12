# TS_STORE_TEST_008_XS

**Purpose:** Flag routing at xFull scale (1M events/run × 3 runs, persist on final run): `KeeperRecord` → jText, `DatabaseEntry` → SQLite on disjoint indices — same verification as 008 TS.

| Compiler | Persist | Output | Records | Duration | Status | Log |
|----------|---------|--------|---------|----------|--------|-----|
| gcc | flags | off | 1000000 | 3.15s | PASS | [log](../flags_logs/TS_STORE_TEST_008_XS/gcc_flags_off.log) |

