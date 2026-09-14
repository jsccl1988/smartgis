<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` — leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically moved out of `src/app/` so that tree only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy/app:app` → `SmartGis.exe` |
| Core DLL | `//src/legacy/app:app_core` (`dll_stem=app_core`, source `smtapp.cpp`) |
| Include prefix | `"legacy/app/…"`（含 `"legacy/app/smtapp.h"`） |
| Gate | `smt_build_app` / `build.bat legacy_app`（日常 `build.bat app` 走 Views） |
| Default `src_all` | **no** |
| 旧路径 | ~~`src/legacy_app/`~~ 已并入本目录（勿再并行维护） |

Do not add new product features here — freeze except compile/path fixes. Destination chrome is `src/app/views` + `src/ui/views`.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/build/src-layout.md`](../../docs/build/src-layout.md).
