<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SDD progress — Plugin host

Plan: `docs/superpowers/plans/2026-09-13-plugin-host.md`
Spec: `docs/superpowers/specs/2026-09-13-plugin-host-design.md`

Branch: `master` (no topic branch, no commits)

## Completed tasks

| Task | Status | Notes |
| --- | --- | --- |
| 1 | complete | Manifest / Registry / leftover adapter |
| 2 | complete | content::PluginHost + command contribute / withdraw |
| 3 | complete | Views form controls + MapPreviewView / AboutDialog |
| 4 | complete | dem Views + register_dem |
| 5 | complete (stub proc) | proj Views; transform factories stubbed |
| 6 | complete | print preview shell |
| 7 | complete | map_service Views |
| 8 | complete | model3d nine commands |
| 9 | complete | baogrid four commands |
| 10 | complete | ManagerView + Views chrome child |
| 11 | partial | Python probe + sample; no CPython C API embed |
| 12 | complete | SHA-256 + tweetnacl ed25519 + zip store tests |
| 13 | complete | ProcessingPool thread + kUtilityStub |
| 14 | complete | README / src-layout / docs index |

## Tests

- `out\plugin_host_test.exe` — exit 0, `plugin_host_test: ok`
- `out\plugin_python_test.exe` — exit 0, `plugin_python_test: ok`
