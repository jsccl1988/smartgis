<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/`

Leftover 2010 MFC / GDI / GL / IATool trees. Not the desktop endgame (Views + Skia under `src/ui/views`, `src/app/{views,winui}`).

| Subdir | Role | Typical GN / DLL |
| --- | --- | --- |
| `app/` | MFC `SmartGis.exe` + `app_core` | `//src/legacy/app:app`（`build.bat legacy_app`） |
| `ui/` | MFC chrome (`gui` / `mfc_ex` / `xview` / …) | `dll_stem=ui_legacy` |
| `render/` | Leftover GDI / GL / render3d / scene3d / … | optional `dll_stem=legacy_render` |
| `tool/` | Leftover `SmtIATool` / `SmtGroupTool` | optional `dll_stem=legacy_tool` |
| `xml/` | TinyXML (from `src/base/core`) | `xml_sources` → product platform DLL |

Layout docs: [`docs/build/src-layout.md`](../../docs/build/src-layout.md). Endgame UI: [`docs/build/ui-views-skia.md`](../../docs/build/ui-views-skia.md).
