# TS_STORE_TEST_008_XS

**Purpose:** Smoke-scale flag routing: 1,000 events, 10 `KeeperRecord` + 10 `DatabaseEntry` on disjoint indices — same verification as 008 TS at smaller N.

| Compiler | Persist | Output | Records | Duration | Status | Log |
|----------|---------|--------|---------|----------|--------|-----|
| gcc | flags | off | 10000 | 0.03s | PASS | [log](../flags_logs/TS_STORE_TEST_008_XS/gcc_flags_off.log) |

