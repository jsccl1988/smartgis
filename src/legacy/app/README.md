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

## China map bootstrap

- 启动时 `SmtApp::DelayInit` / `InitSmtMap` 优先加载 `out/china_city.gpkg`（`area`/`line`/`point`/`text` 四层地级底图），缺包再试 `china_city.geojson` / `china_plp.geojson`。面按 `name`/`adcode` 哈希分色；注记用 YaHei + UTF-8 `TextOutW`。
- `--self-test`：断言图层 ≥1 且要素 ≥3，写 `china-plp-ok`；Edit 视图创建后若 BCG 卡住，由 `CSmartMapEditView::OnCreate` 看门狗 `TerminateProcess(0)`。
- **交互**：`InitInstance` 先显示主框，再 `DelayInit`（只打开一次 `china_city`），再 **`PostMessage(ID_WND_MAPEDIT)`** 延后开 Edit 2D——避免在无外层消息泵时同步 `OpenDocumentFile` 导致 BCG MDI Tab 在 `CView::OnInitialUpdate` 死锁（标题栏「未响应」）。**不**在启动时拉 3D。Data / 3D 按需打开。`--self-test` 仍同步开 Edit（看门狗兜底）。
- **开 3D**：菜单 **窗口(&W) → 三维窗口(&3)**（`ID_WND_3D`）。动态视图菜单会替换 RC 菜单，因此该弹出项挂在 `append_mdi_window_menu` 上。
- **3D**：`Smt3DXView::CreateRender` 把 `china_city.gpkg` 抬进 leftover GL；DEM 掩膜走 `gis::land_mask` 的 bbox 加速。无样本时回退立方体。`gl_map_paint_test` 断言非黑像素。

Do not add new product features here — freeze except compile/path fixes. Destination chrome is `src/app/views` + `src/ui/views`.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/build/src-layout.md`](../../docs/build/src-layout.md).

---

**最后更新：** 2026-09-19
