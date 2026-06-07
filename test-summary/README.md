# Test Summary Hub

Committed lightweight results promoted from `test-results/`. Each leaf links to a per-run README with per-test detail pages under `by_test/`.

| OS | Disk | Size | Compilers | Scenarios | Run (UTC) | Detail |
|----|------|------|-----------|-----------|-----------|--------|
| OS_003 | ssd | Smoke | gcc,clang | 226/226 | 2026-06-07T14:11:07Z | [README](OS_003/ssd/Smoke/README.md) |
| OS_003 | x7k | Smoke | gcc,clang | 224/224 † | 2026-06-07T13:38:41Z | [README](OS_003/x7k/Smoke/README.md) |

† Predates `flags=x` in `tests/test_params.txt`; re-run + promote for 226/226 (see FORWARDING.md).

Regenerate: `./scripts/promote_summaries.sh --all` (updates hub automatically).
