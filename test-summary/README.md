# Test Summary Hub

Committed lightweight results promoted from `test-results/`. Each leaf links to a per-run README with per-test detail pages under `by_test/`.

| OS | Compiler | Disk | Size | Scenarios | Run (UTC) | Detail |
|----|----------|------|------|-----------|-----------|--------|
| OS_001 | clang | SSD | SMOKE | 113/113 | 2026-06-09T03:12:36Z | [README](OS_001/clang/ssd/Smoke/README.md) |
| OS_001 | gcc | SSD | SMOKE | 113/113 | 2026-06-09T03:12:27Z | [README](OS_001/gcc/ssd/Smoke/README.md) |
| OS_003 | GCC | X7K | XFULL | 113/113 | 2026-06-07T15:31:47Z | [README](OS_003/x7k/xFull/README.md) |
| OS_003 | GCC,CLANG | 10K | SMOKE | 226/226 | 2026-06-07T15:18:27Z | [README](OS_003/10k/Smoke/README.md) |
| OS_003 | GCC,CLANG | 10K | XFULL | 226/226 | 2026-06-07T15:22:41Z | [README](OS_003/10k/xFull/README.md) |
| OS_003 | GCC,CLANG | SSD | SMOKE | 226/226 | 2026-06-09T02:42:35Z | [README](OS_003/ssd/Smoke/README.md) |
| OS_003 | GCC,CLANG | SSD | XFULL | 226/226 | 2026-06-09T02:29:44Z | [README](OS_003/ssd/xFull/README.md) |
| OS_003 | GCC,CLANG | X7K | SMOKE | 226/226 | 2026-06-07T17:48:15Z | [README](OS_003/x7k/Smoke/README.md) |

Regenerate: `./scripts/promote_summaries.sh --all` (updates hub automatically).
