<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# App 终局升级 + MFC 迁出 `legacy_app` / `legacy_ui`

**Date:** 2026-09-14  
**Status:** accepted（Phase 1+2 已落地；日常停编 MFC，opt-in `build.bat legacy_app`）  
**Goal:** `src/app` 只保留终局/原型宿主；MFC `SmartGis.exe` 与 MFC chrome 树物理迁出；Views 主入口按编辑→属性表→Catalog→Ambox/插件→3D 升级；对等验收后停编 MFC 壳（源码保留）。  
**Related:** [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md)、[`2026-09-14-ui-leftover-chrome-parity-design.md`](2026-09-14-ui-leftover-chrome-parity-design.md)、[`docs/build/src-layout.md`](../../build/src-layout.md)、[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)、对称先例 [`archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md) / [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md)  
**Plan:** [`../plans/2026-09-14-app-legacy-split.md`](../plans/2026-09-14-app-legacy-split.md)

## Locked decisions

| Topic | Choice |
| --- | --- |
| Approach | 对称 leftover 树（仿 `legacy_tool` / `legacy_render`） |
| MFC app + `app_core` | → `src/legacy_app/` |
| MFC UI chrome | → `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` |
| Endgame `src/app/` | 仅 `views/` + `winui/`（winui 仍为非终局原型） |
| Include / GN | **无转发头**；一次性改 `legacy_app/`、`legacy_ui/` |
| DLL stem | `app_core` 不变；`ui_legacy` stem **不变**（部署名稳定）；GN 路径改 `//src/legacy_ui:ui_legacy` |
| Exe | 日常 `build.bat app` → `SmartGisViews.exe`；MFC `SmartGis.exe` → `build.bat legacy_app` |
| Stop compile MFC | **已闸门**：默认不编；源码保留于 `legacy_app` / `legacy_ui` |
| Phase 2 order | 1 编辑工作流 → 2 属性表可写 → 3 Catalog 真数据 → 4 Ambox/插件 → 5 3D HWND |
| Parallel land | Phase 1 与 Phase 2 可并行（不重叠路径）；Phase 2 内 1–3 优先并行，4–5 紧随 |
| Qt / dock / MDI | 禁止 |
| Commit | 仅用户明确要求时 |

## Non-goals

- 本轮不删除 leftover 源码  
- 不把 WinUI / Feature Pack 升为终局  
- 不 vendor Chromium / 完整 Skia  
- 不在 leftover 树加新功能（只修编译 / 路径）  
- 不改 `dll_stem=ui_legacy` / `app_core` 部署名（仅路径与 GN label）

## Target trees

### `src/app/`（终局 + 原型）

```
src/app/
  views/          SmartGisViews.exe（组合层）
  winui/          SmartGisWinui.exe（原型）
  BUILD.gn        group 转发 / 可选空壳说明；不再挂 MFC sources
  README.md
```

### `src/legacy_app/`（MFC 壳）

```
src/legacy_app/
  BUILD.gn                 smt_mfc_executable("app") → SmartGis.exe
  app_core/                SmtApp DLL（dll_stem=app_core）
  main_frame.* / child_frame.* / smart_* / stdafx.* / resource.* / res/ / *.rc
  group("legacy_app_all")
```

Include：`"legacy_app/…"`、`"legacy_app/app_core/…"`。  
GN：`//src/legacy_app:app`、`//src/legacy_app/app_core:app_core`。  
根 `//:smartgis` / `build.bat app` 改指新 label。

### `src/legacy_ui/`（MFC chrome）

```
src/legacy_ui/
  BUILD.gn                 smt_shared_library("ui_legacy") dll_stem=ui_legacy
  gui/ mfc_ex/ xview/ xcatalog/ xambox/ chart/
  group("ui") → :ui_legacy   # 保持旧 group 名习惯时可 group 转发
```

Include：`"legacy_ui/gui/…"` 等（原 `"ui/gui/…"` → `"legacy_ui/gui/…"`）。  
`src/ui/views/` **不动**（终局工具箱仍在 `src/ui/views`）。  
`//src/ui:ui_legacy` 变为 **group 转发**到 `//src/legacy_ui:ui_legacy`，或根 BUILD 直接改 deps（推荐转发一轮，减少漏改）。

### `src/ui/`（迁出后）

```
src/ui/
  views/          唯一终局工具箱
  BUILD.gn        仅 views / 转发 group；不再编 MFC sources
```

## Phase 1 — 物理迁出

1. `git mv` MFC app 根文件 + `app_core` → `src/legacy_app/`  
2. `git mv` `src/ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` → `src/legacy_ui/`  
3. 全库改 include / GN label（scoped search；无 `"ui/gui/` 等残留于产品代码）  
4. 更新 `docs/build/src-layout.md`、`ui-views-skia.md`、migration 规格路径行  
5. `build.bat app` 仍可加载 `SmartGis.exe`（允许修编译）  
6. `build.bat views` 不受 MFC 路径影响（views 不链 leftover UI）

**Done when：** `src/app/` 无 MFC 壳源；`src/ui/` 无六棵 MFC 子树；`ninja`/`build.bat app` 与 `views` 可分别构建；文档路径一致。

**Phase 1 状态（2026-09-14）：已完成。** 迁出后 `build.bat legacy_app` / `views` 均可绿；产物含 `SmartGis.exe`（opt-in）、`ui_legacy_d.dll`、`app_core_d.dll`、`SmartGisViews.exe`。

## Phase 2 — Views 主入口能力（超出 chrome parity）

在 [`2026-09-14-ui-leftover-chrome-parity-design.md`](2026-09-14-ui-leftover-chrome-parity-design.md) 组合闭环之上，按序交付：

| # | Capability | Acceptance (Views 路径) |
| --- | --- | --- |
| 1 | 编辑工作流 | 选择/绘制草稿/提交经 `ViewHost` + `tool::Workspace` / `sdb::EditSession`；状态栏可见结果 |
| 2 | 属性表可写 | `AttributeTable` 单元格或批量写回（字符串/opaque id 边界；控件不持 `SmtFeature*`）；有单测或 `--self-test` |
| 3 | Catalog 真数据 | 开图后 LayerTree/Catalog 反映真实图层显隐/激活；少依赖纯 demo |
| 4 | Ambox / 插件 | `CommandCatalog` / `PluginHost` 枚举灌组；插件命令少弹 leftover `CDlg*` |
| 5 | 3D 页 | Tab「3D」稳定子 HWND + 基本漫游；无设备时占位不崩 |

并行策略：1–3 可与 Phase 1 并行（改 `src/app/views`、`src/ui/views`、`src/tool`、`src/content`）；4–5 依赖 1–3 接线稳定后紧随。路径冲突时 Phase 1 优先改 include，Phase 2 避开 `legacy_*` 业务改动。

## Stop-compile gate（已落地）

1–5 验收 + `views_unittests` / `SmartGisViews.exe --self-test` 绿后已执行：

1. `build.bat app` → `views`（`SmartGisViews.exe`）  
2. MFC：`build.bat legacy_app` / `smartgis` → `//:legacy_app_all`（`smt_build_app`）  
3. `legacy_ui` 仍仅随 `smt_build_app` / `build.bat ui_legacy`  
4. 源码删除另开变更（本规格不授权）

## Dependency / layering

```
SmartGisViews.exe (src/app/views)
  → ui::views (src/ui/views)
  → content::ViewHost / MapContents
  → tool::Workspace / CommandCatalog
  → sdb / render (endgame)
  ✗ 不 deps legacy_ui / legacy_app

SmartGis.exe (src/legacy_app)
  → legacy_app/app_core
  → legacy_ui (ui_legacy.dll)
  → legacy_tool / legacy_render / …
```

Forbidden: endgame `views` → `legacy_ui`；`src/ui/views` → MFC headers。

## Testing

| Entry | Role |
| --- | --- |
| `build.bat app` / `views` | 终局壳 `SmartGisViews.exe` |
| `build.bat legacy_app` | opt-in MFC `SmartGis.exe` |
| `views_unittests` | 工具箱 + AttributeTable 写回等 |
| `SmartGisViews.exe --self-test` | 壳 e2e |

## Docs to update (same change sets)

- `docs/build/src-layout.md` — App / UI 行  
- `docs/build/ui-views-skia.md` — leftover 路径  
- `2026-09-13-ui-views-mfc-migration-design.md` — leftover 落点改为 `legacy_*`  
- `src/README.md`、`src/app/README.md`（若无则新建）、`src/legacy_app/README.md`、`src/legacy_ui/README.md`  
- 本规格 Status 随 Phase 推进为 `accepted` / 归档时 `landed`

## Out of scope

- 像素级 BCG 复刻  
- 真 dock / MDI  
- Qt  
- 强制 `git commit`（需用户点名）

---

**最后更新：** 2026-09-14（Phase 2 + stop-compile gate）
