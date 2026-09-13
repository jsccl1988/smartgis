# SDD progress — Base IPC / ContentMain / GPU

Plan: `docs/superpowers/plans/2026-09-13-base-ipc-mojom.md`
Spec: `docs/superpowers/specs/2026-09-13-base-ipc-mojom-design.md`

Branch: `master` (no topic branch). OGR used `.superpowers/sdd/progress.md`; this file is the IPC ledger.

## Completed tasks

| Task | Status | SHA | Subject |
| --- | --- | --- | --- |
| 1 | complete | `16f3cb9` | Bump the tree to C++23. |
| 2 | complete | `1901382` | Add ContentMain process-type dispatch. |
| 3 | complete | `f2aa569` | Relaunch the chrome PE as --type=gpu. |
| 4 | complete | `88bc92e` | Split GpuMain from renderer entry. |
| 5 | complete | `79b0075` | Rename MapSession to MapContents. |
| 6 | complete | `187ff14` | Document ContentMain and the standalone GPU process. |
| 7 | pickle wire (landed) | — | Named pipe + mogu BinarySink/pickle; C++ `archive()` structs. No chromium, no mojom. |

## Tests

- `build.bat views` exit 0
- `out\SmartGisViews.exe --type=gpu --self-test` exit 0 (`d3d=1`)
- `out\SmartGisViews.exe --type=renderer --self-test` exit 0 (`no GPU device`)
