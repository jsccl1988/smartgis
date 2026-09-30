<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/`

Leftover 2010 MFC / GDI / GL / IATool trees. Not the desktop endgame (Views + Skia under `src/ui/views`, `src/app/{views,winui}`).

| Subdir | Role | Typical GN / DLL |
| --- | --- | --- |
| `app/{core,shell,doc,view,res}` | MFC `SmartGis.exe` + `app_core` | `//src/legacy/app:app`（`build.bat legacy_app`） |
| `ui/` | MFC shell (`gui` / `mfc_ex` / `xview` / …) | `dll_stem=ui_legacy` |
| `render/` | Leftover GDI / GL / rhi / scene3d / … | optional `dll_stem=legacy_render` |
| `tool/` | Leftover IATool：`{abi,msg,nav,select,draft,base,factory}`（终局浅镜像）；`msg` = `GT_MSG_*` → `tool::Workspace`（`source_set`，不进 DLL） | optional `dll_stem=legacy_tool`；`//src/legacy/tool/msg:adapter` |
| `plugin/` | Leftover AuxModule + domain MFC shells; `runtime/{auxmodule,bridge}` + `product/<domain>/{shell,views}` | `dll_stem=plugin` + `plugin_{dem,proj,print,model3d,orthogrid}`; `runtime:bridge` |
| `core/` | Leftover Smt listener / command / msg / api / structs / `core.h` (from `src/base/core`) | `core_sources` → product platform DLL |
| `sys/` | Leftover `SmtSysManager` | `sys_sources` → product platform DLL |

XML: TinyXML under `legacy/xml` removed; use `//third_party:pugixml`.

Layout docs: [`docs/build/src-layout.md`](../../docs/build/src-layout.md). Endgame UI: [`docs/build/ui-views-skia.md`](../../docs/build/ui-views-skia.md).
