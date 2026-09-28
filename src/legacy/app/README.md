<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` — leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically under `src/legacy/app/` so `src/app/` only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy/app:app` → `SmartGis.exe` |
| Core DLL | `//src/legacy/app:app_core` (`dll_stem=app_core`, source `core/smtapp.cpp`) |
| Include prefix | `"legacy/app/<module>/…"` — e.g. `"legacy/app/core/smtapp.h"`, `"legacy/app/shell/smart_gis.h"`, `"legacy/app/doc/smart_gis_doc.h"`, `"legacy/app/view/smart_map_edit_view.h"`; PCH stays `"legacy/app/stdafx.h"` |
| Gate | `smt_build_app` / `build.bat legacy_app`（日常 `build.bat app` 走 Views） |
| Default `src_all` | **no** |
| Frozen labels | `:app` / `:app_core` / `:legacy_app_all`；`APP_CORE_EXPORT` / pragma `app_core(_d).lib` |

## Layout (approach C — capability subdirs)

```
src/legacy/app/
  BUILD.gn  README.md  stdafx.*  resource.h  smart_gis.rc
  core/     smtapp.*                 → app_core DLL
  shell/    smart_gis.* main_frame.* child_frame.*
  doc/      smart_gis_doc.*
  view/     smart_*_view.*           (thin CView / Smt*XView forwarders)
  res/      icons / cursors / rc2
```

Break includes (tool-style): headers moved with implementation; **no** shim at the old flat `legacy/app/*.h` paths.

Design: [`docs/superpowers/specs/2026-09-27-legacy-app-subdirectory-layout-design.md`](../../docs/superpowers/specs/2026-09-27-legacy-app-subdirectory-layout-design.md), plan [`docs/superpowers/plans/2026-09-27-legacy-app-subdirectory-layout.md`](../../docs/superpowers/plans/2026-09-27-legacy-app-subdirectory-layout.md).

## SP3 strangler (host behavior)

| Concern | Landing |
| --- | --- |
| Attribute / Catalog JSON | Already `content::feature_attrs` / `catalog_layers` + `app::MapScene` |
| Sample / china path policy | `content::resolve_sample_map_candidates` / `try_resolve_existing_sample_map` — `SmtApp` calls them |
| GDAL open + leftover mapmgr / style | Still orchestrated in `core/smtapp.cpp` (leftover singletons) |
| Draft commit HWND-free seam | Deferred — see `TODO(sp3)` / plan Task 5; viewport math stays in `app::MapScene` |
| MFC view paint / SetOperMap | Thin `view/*` forwarders over `legacy/ui/xview` |

## China map bootstrap

- 启动时 `SmtApp::DelayInit` / `InitSmtMap` 优先加载 `out/china_city.gpkg`（`area`/`line`/`point`/`text` 四层地级底图），缺包再试 `china_city.geojson` / `china_plp.geojson`。面按 `name`/`adcode` 哈希分色；注记用 YaHei + UTF-8 `TextOutW`。
- `--self-test`：断言图层 ≥1 且要素 ≥3，写 `china-plp-ok`；Edit 视图创建后若 BCG 卡住，由 `CSmartMapEditView::OnCreate` 看门狗 `TerminateProcess(0)`。
- **交互**：`InitInstance` 先显示主框，再 `DelayInit`（只打开一次 `china_city`），再 **`PostMessage(ID_WND_MAPEDIT)`** 延后开 Edit 2D——避免在无外层消息泵时同步 `OpenDocumentFile` 导致 BCG MDI Tab 在 `CView::OnInitialUpdate` 死锁（标题栏「未响应」）。**不**在启动时拉 3D。Data / 3D 按需打开。`--self-test` 仍同步开 Edit（看门狗兜底）。
- **开 3D**：菜单 **窗口(&W) → 三维窗口(&3)**（`ID_WND_3D`）。动态视图菜单会替换 RC 菜单，因此该弹出项挂在 `append_mdi_window_menu` 上。
- **3D**：`Smt3DXView::CreateRender` 把 `china_city.gpkg` 抬进 leftover GL；DEM 掩膜走 `gis::land_mask` 的 bbox 加速。无样本时回退立方体。`gl_map_paint_test` 断言非黑像素。

Do not add new product features here — freeze except compile/path fixes. Destination shell is `src/app/views` + `src/ui/views`.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md`](../../docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md), [`docs/build/src-layout.md`](../../docs/build/src-layout.md).

---

**最后更新：** 2026-09-27
