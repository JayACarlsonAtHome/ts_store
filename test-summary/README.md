# Test Summary Hub

Committed lightweight results promoted from `test-results/`. Each leaf links to a per-run README with per-test detail pages under `by_test/`.

| OS | Compiler | Disk | Size | Scenarios | Run (UTC) | Detail |
|----|----------|------|------|-----------|-----------|--------|
| OS_001 | clang | ssd | Smoke | 113/113 | 2026-06-09T04:34:01Z | [README](OS_001/clang/ssd/Smoke/README.md) |
| OS_001 | clang | ssd | xFull | 113/113 | 2026-06-09T03:23:36Z | [README](OS_001/clang/ssd/xFull/README.md) |
| OS_001 | gcc | ssd | Smoke | 113/113 | 2026-06-09T04:33:51Z | [README](OS_001/gcc/ssd/Smoke/README.md) |
| OS_001 | gcc | ssd | xFull | 113/113 | 2026-06-09T03:23:21Z | [README](OS_001/gcc/ssd/xFull/README.md) |
| OS_003 | gcc | x7k | xFull | 113/113 | 2026-06-07T15:31:47Z | [README](OS_003/x7k/xFull/README.md) |
| OS_003 | gcc,clang | 10k | Smoke | 226/226 | 2026-06-07T15:18:27Z | [README](OS_003/10k/Smoke/README.md) |
| OS_003 | gcc,clang | 10k | xFull | 226/226 | 2026-06-07T15:22:41Z | [README](OS_003/10k/xFull/README.md) |
| OS_003 | gcc,clang | ssd | Smoke | 226/226 | 2026-06-09T02:42:35Z | [README](OS_003/ssd/Smoke/README.md) |
| OS_003 | gcc,clang | ssd | xFull | 226/226 | 2026-06-09T02:29:44Z | [README](OS_003/ssd/xFull/README.md) |
| OS_003 | gcc,clang | x7k | Smoke | 226/226 | 2026-06-07T17:48:15Z | [README](OS_003/x7k/Smoke/README.md) |

Regenerate: `./scripts/promote_summaries.sh --all` (updates hub automatically).
