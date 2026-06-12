# TS_STORE_TEST_008_TS

**Purpose:** Flag-selective persistence at scale: 10,000 in-memory events, 100 with `KeeperRecord` (jText file) and 100 with `DatabaseEntry` (SQLite) on disjoint indices — proves sinks honor flags and measures throughput when most events skip durable I/O.

| Compiler | Persist | Output | Records | Duration | Status | Log |
|----------|---------|--------|---------|----------|--------|-----|
| gcc | flags | off | 10000 | 0.03s | PASS | [log](../flags_logs/TS_STORE_TEST_008_TS/gcc_flags_off.log) |

