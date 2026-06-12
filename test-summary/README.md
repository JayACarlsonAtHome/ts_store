# Test Summary Hub

Committed lightweight results promoted from `test-results/`. Each leaf links to a per-run README with per-test detail pages under `by_test/`.

| OS | Compiler | Disk | Size | Hardware | Scenarios | Run (UTC) | Detail |
|----|----------|------|------|----------|-----------|-----------|--------|
| OS_001 | clang | ssd | Smoke | — | 113/113 | 2026-06-09T04:34:01Z | [README](OS_001/clang/ssd/Smoke/README.md) |
| OS_001 | clang | ssd | xFull | — | 113/113 | 2026-06-09T03:23:36Z | [README](OS_001/clang/ssd/xFull/README.md) |
| OS_001 | gcc | ssd | Smoke | — | 113/113 | 2026-06-09T04:33:51Z | [README](OS_001/gcc/ssd/Smoke/README.md) |
| OS_001 | gcc | ssd | xFull | — | 113/113 | 2026-06-09T03:23:21Z | [README](OS_001/gcc/ssd/xFull/README.md) |
| OS_003 | clang | ssd | Smoke | 8 cores (1 physical), 31,936 MiB RAM — Intel(R) Core(TM) i7-7820HQ CPU @ 2.90GHz @ 3,596 MHz max | 113/113 | 2026-06-12T03:33:49Z | [README](OS_003/clang/ssd/Smoke/README.md) |
| OS_003 | clang | ssd | xFull | 8 cores (1 physical), 31,936 MiB RAM — Intel(R) Core(TM) i7-7820HQ CPU @ 2.90GHz @ 3,598 MHz max | 113/113 | 2026-06-12T03:38:30Z | [README](OS_003/clang/ssd/xFull/README.md) |
| OS_003 | gcc | ssd | Smoke | 8 cores (1 physical), 31,936 MiB RAM — Intel(R) Core(TM) i7-7820HQ CPU @ 2.90GHz @ 3,502 MHz max | 113/113 | 2026-06-12T03:33:02Z | [README](OS_003/gcc/ssd/Smoke/README.md) |
| OS_003 | gcc | ssd | xFull | 8 cores (1 physical), 31,936 MiB RAM — Intel(R) Core(TM) i7-7820HQ CPU @ 2.90GHz @ 3,599 MHz max | 113/113 | 2026-06-12T03:37:28Z | [README](OS_003/gcc/ssd/xFull/README.md) |

Regenerate: `./scripts/promote_summaries.sh --all` (updates hub automatically).
