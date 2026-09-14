<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy_ui` — leftover MFC chrome

Six MFC UI trees merged into one DLL (`dll_stem=ui_legacy`). Physically moved out of `src/ui/` so that tree only holds the endgame toolkit (`views/`).

| Tree | Role |
| --- | --- |
| `gui/` | Dialogs / dock bars |
| `mfc_ex/` | Feature Pack helpers + grid |
| `xview/` | Map / 3D MFC views |
| `xcatalog/` | Layer / map / scene catalogs |
| `xambox/` | Aux-module box |
| `chart/` | Stat chart (`stat_chart_sources`) |

| Item | Value |
| --- | --- |
| GN | `//src/legacy_ui:ui_legacy` |
| Forwarder | `//src/ui:ui_legacy` → above |
| Include prefix | `"legacy_ui/gui/…"` etc. (no `"ui/gui/…"` forward headers) |
| Gate | `smt_build_app` / `build.bat ui_legacy` / pulled by `SmartGis.exe` |
| Default `src_all` | **no** |

`tool_group_sources` still compile into this DLL to avoid a link cycle with `legacy_tool`. Endgame Views must not depend on this tree.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/build/ui-views-skia.md`](../../docs/build/ui-views-skia.md).
