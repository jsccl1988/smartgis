<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP3 — Host 行为提取（Catalog / edit / attrs，无 HWND）

**Status:** active  
**Date:** 2026-09-19  
**Goal:** 从 `legacy/ui` + `legacy/app`（及已迁入 `app::MapScene` 的会话逻辑）抽出 **HWND-free** 宿主行为单元，供 Views / CEF / WinUI 经 `content/public` 调用；MFC 对话框保持薄壳。  
**Related:** [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md)（SP0）、[`2026-09-14-app-legacy-split-design.md`](2026-09-14-app-legacy-split-design.md) Phase 2、[`2026-09-14-ui-leftover-chrome-parity-design.md`](2026-09-14-ui-leftover-chrome-parity-design.md)、[`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md)、[`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md)  
**Plan:** [`../plans/2026-09-19-legacy-host-behavior-extract.md`](../plans/2026-09-19-legacy-host-behavior-extract.md)

## Non-goals

- 不破 leftover DLL `dll_stem` / `Smt_*` 导出（本子规格无 leftover-only break）。
- 禁止 Qt；禁止 Feature Pack / dock / MDI 作为终局。
- 不 wholesale rewrite `legacy/ui` 或 `legacy/app`；不吞并 SP1（tool）/ SP2（present）/ SP4（scene3d）。
- 不把 `EventBus` 迁入 `src/tool`；不让 `ui::views` 控件持有 `SmtFeature*` / `OGRFeature*`。
- 不与 sibling 伞状 / tool-workspace / present-facade 规格文件名打架；本文件只覆盖 **Host 行为**。

## Locked decisions

| Topic | Choice |
| --- | --- |
| 手法 | Facade strangler：纯逻辑进 `content`（或 `sdb`）；`app::MapScene` / MFC dlg 变薄适配 |
| 本轮热点（选 2，落地 1） | **(A) Attribute writeback**（已落地）；**(B) Catalog layer snapshot**（本增量落地） |
| Attribute 目标 API | `content::encode_feature_token` / `decode_feature_token` / `apply_named_field` |
| Catalog 目标 API | `content::LayerDesc` + `layers_to_catalog_json`（`MapScene` / CEF / Views sync 共用） |
| Edit draft commit | 暂留 `MapScene::append_from_draft`（依赖 pan/scale 视口）；不在本轮拆 |
| Chrome 边界 | 控件只传 string / opaque token；写回经 host 回调 → content 纯函数 → `EditSession::commit` |
| 与 chrome-parity | **扩展** Phase 2「属性表可写 / Catalog 真数据」，不另立第二套图例协议 |

## Path ownership

| May edit | Must not edit (siblings) |
| --- | --- |
| `docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md` (+ plan) | SP1/SP2/SP4/SP5 专属规格文件名与其默认树 |
| `src/content/**`（本轮 `feature_attrs*` + `catalog_layers*`） | `src/legacy/tool/**`、`src/legacy/render/**` |
| `src/app/views/**`（`map_scene` / `browser_view` 接线） | 无协调时改 SP1/SP2 热点 |
| `src/app/cef/**`（Catalog JSON 委托 `layers_to_catalog_json`） | — |
| `src/legacy/ui/**` / `src/legacy/app/**`（仅抽调用点 / 注释指向终局 API） | 借机加 Feature Pack 功能 |
| 可选后续：`src/gis/**`、`src/ui/views/**` helpers | — |

## Dependency

```
ui::views AttributeTable / CatalogView
        → app::BrowserView / MapScene（薄壳）
        → content::feature_attrs（HWND-free）
        → gis::EditSession::commit（已有）

legacy CDlg2DFeatureInfo（MFC）
        → 仍持 SmtFeature* 只读展示；写回不经 HWND；产品路径走 content
```

终局 **不得** `#include "legacy/…"`。legacy → `content/public` 单向允许。

## ABI

- leftover 导出与 `dll_stem`：**不变**。
- 新 API：两层命名空间 `content`，函数 `snake_case`。

## Hotspot analysis（现状）

| 热点 | 现状 | SP3 动作 |
| --- | --- | --- |
| Attribute writeback | `MapScene::update_feature_field` + `AttributeTable::on_cell_commit` 已接线；token 编解码在 `MapScene` 与 `browser_view` detail 重复；`MapScene` 因 `paint(HDC)` 拉入 `windows.h` | 抽出 **HWND-free** `content::feature_attrs`；`MapScene` 委托；删 detail 死代码 |
| Catalog true data | `MapScene::open_path` / `layer_descs` + `sync_catalog_from_scene` 已有真图层；`CatalogCall` 仍是管道 stub；CEF 曾自建 JSON | 抽出 `content::LayerDesc` + `layers_to_catalog_json`；CEF / Views 委托 |
| Edit draft commit | `append_from_draft` + `EditCommitted` 已工作；坐标变换绑在 scene 视口 | 延后；避免与 SP1 tool strangler 抢缝 |

## Success criteria

1. [x] Living design 含 Goal / Non-goals / Locked / Path / Dependency / ABI / Success / Out of scope。  
2. [x] `content::encode_feature_token` / `decode_feature_token` / `apply_named_field` 存在且 **无** Win32 / HWND 依赖。  
3. [x] `MapScene::{feature_token,feature_id_from_token,update_feature_field}` 委托上述 API。  
4. [x] `content_feature_attrs_test`（或等价 test target）覆盖 round-trip token + field apply / reject。  
5. [x] Views 属性表写回路径仍经 `document_.update_feature_field`（行为不回归）。  
6. [x] 默认产品壳不新增 `legacy/**` include。  
7. [x] `content::LayerDesc` + `layers_to_catalog_json` 存在且无 HWND；CEF / Views Catalog sync 委托。  
8. [x] `content_catalog_layers_test` 覆盖空数组、转义、不含 `active` 字段。  
9. [ ] Edit draft commit 仍延后；`CatalogCall` 管道仍为 stub（仅 snapshot JSON 抽出）。

## Out of scope for later SPs

- SP1：leftover tool → `Workspace` 适配。  
- SP2：Present / Paint Facade（含把 `MapScene::paint` 迁出的 GDI 缝）。  
- SP4：`scene3d` → World / GpuScene。  
- SP5：默认停编 leftover 闸门文档化。  
- Grid 单元格 UI 细节：已在 `ui::views::AttributeTable`；本规格不重做控件。

## Done when（本轮 + Catalog 增量）

- Spec + plan 落地；Attribute 热点抽出 + 单测；`MapScene` 接线。  
- Catalog `LayerDesc` JSON 抽出 + 单测；CEF / Views sync 委托。  
- Edit draft / CatalogCall 管道仍延后。  
- 不 commit / 不新开分支 / 不强制 reindex。
