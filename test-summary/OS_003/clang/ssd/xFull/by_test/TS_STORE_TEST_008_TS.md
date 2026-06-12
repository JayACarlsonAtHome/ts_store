# TS_STORE_TEST_008_TS

**Purpose:** Flag-selective persistence at scale: 1,000,000 in-memory events × 3 runs (persist on final run only), 10,000 `KeeperRecord` → jText and 10,000 `DatabaseEntry` → SQLite on disjoint indices (1% each) — proves sinks honor flags and measures hot-path throughput when most events skip durable I/O.

| Compiler | Persist | Output | Records | Duration | Status | Log |
|----------|---------|--------|---------|----------|--------|-----|
| clang | flags | off | 1000000 | 1.21s | PASS | [log](../flags_logs/TS_STORE_TEST_008_TS/clang_flags_off.log) |

