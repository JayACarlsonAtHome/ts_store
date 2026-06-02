# ts_store Stress Test Results — Clang (clang++ 21)

**Run date**: 2026-06-02 07:42:32 UTC
**Compiler**: Clang 21.1.8 (Red Hat)
**Output mode**: silent (logs only)
**Total tests**: 15 (001-007 TS/XS + flags)
**Passed**: 15
**Failed**: 0

## Summary Table

| Test | Status | Log |
|------|--------|-----|
| ts_store_001_TS | ✅ PASS | [log](ts_store_001_TS.log) |
| ts_store_001_XS | ✅ PASS | [log](ts_store_001_XS.log) |
| ts_store_002_TS | ✅ PASS | [log](ts_store_002_TS.log) |
| ts_store_002_XS | ✅ PASS | [log](ts_store_002_XS.log) |
| ts_store_003_TS | ✅ PASS | [log](ts_store_003_TS.log) |
| ts_store_003_XS | ✅ PASS | [log](ts_store_003_XS.log) |
| ts_store_004_TS | ✅ PASS | [log](ts_store_004_TS.log) |
| ts_store_004_XS | ✅ PASS | [log](ts_store_004_XS.log) |
| ts_store_005_TS | ✅ PASS | [log](ts_store_005_TS.log) |
| ts_store_005_XS | ✅ PASS | [log](ts_store_005_XS.log) |
| ts_store_006_TS | ✅ PASS | [log](ts_store_006_TS.log) |
| ts_store_006_XS | ✅ PASS | [log](ts_store_006_XS.log) |
| ts_store_007_TS | ✅ PASS | [log](ts_store_007_TS.log) |
| ts_store_007_XS | ✅ PASS | [log](ts_store_007_XS.log) |
| ts_store_flags | ✅ PASS | [log](ts_store_flags.log) |

## Notes

- Tests 005 and 007 are the large-scale "massive" tests (historically ~1,000,000 records per run × 50 runs; the THREADS/EVENTS_PER_THREAD limits are configurable — see source comments in the test files for history of changing the limit from time to time).
- Double-buffered persistence (using BinaryEventSink + DoubleBufferedWriter) is enabled for the 005/007 runs (as updated for the "big test with double buffering" request). The hot path stays fast; background thread drains.
- All other tests exercise core features (flags handling, different scales, timestamped vs non-timestamped variants).
- All structural verifications passed.
- Logs contain the complete output from each test binary (some are large due to debug-style dumps in lower-numbered tests).
- See the sibling `gcc/summary.md` for GCC 15 results.
- The top-level README.md summarizes the test matrix and links to both compiler-specific pages.

Raw logs and this summary are in this directory. All tests were driven by the automation in `scripts/run_all_tests.sh` (supports --compiler and --output yes/no for console vs logs-only selection).
