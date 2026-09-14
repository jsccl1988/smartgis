<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy_app` — leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically moved out of `src/app/` so that tree only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy_app:app` → `SmartGis.exe` |
| Core DLL | `//src/legacy_app/app_core:app_core` |
| Include prefix | `"legacy_app/…"` / `"legacy_app/app_core/…"` |
| Gate | `smt_build_app` / `build.bat app` |
| Default `src_all` | **no** |

Do not add new product features here — freeze except compile/path fixes. Destination chrome is `src/app/views` + `src/ui/views`.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/build/src-layout.md`](../../docs/build/src-layout.md).
