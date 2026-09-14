<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views 迁移 leftover MFC chrome

**Date:** 2026-09-13  
**Status:** active  
**Scope:** 目标架构与文件所有权。能力按模块并行落地（不再按原 Phase 1→2→3 串行闸门）。本文件是该主题的架构权威；控件工具箱细节见 [`2026-09-13-ui-views-controls-design.md`](2026-09-13-ui-views-controls-design.md)；组合优先对等缺口见 [`2026-09-14-ui-leftover-chrome-parity-design.md`](2026-09-14-ui-leftover-chrome-parity-design.md)。产品 as-built 入口：[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)。

## 目标

把 leftover MFC 桌面壳（`SmartGis.exe` + `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` + `src/legacy_app`）重写到 **`ui::views`**。产品入口只保留 **`SmartGisViews.exe`**。地图继续挂 HWND，经 `content::ViewHost` 派发；**不**包装 `CView`。达到功能对等后停止编译 leftover MFC UI。本轮不删除 leftover 源码。

## 锁定决策

| 主题 | 选择 |
| --- | --- |
| 产品入口 | 仅 `SmartGisViews.exe`。`SmartGis.exe` leftover 直到 parity，然后停止编译 MFC UI。 |
| Chrome | 单窗口、可拖拽 `Splitter`（不是真 dock，不是 MDI）。 |
| Tabs | Map edit 2D / Datasource / 3D。 |
| 路径 | 在 `ui::views` 重写 chrome；地图挂 HWND + `content::ViewHost`。不包装 `CView`。 |
| 落地节奏 | 原计划串行分阶段；现按能力并行落地。规格写目标架构与文件所有权，不写“必须等上一阶段合并”。 |
| UI 工具箱 | 禁止 Qt。Views + Skia 是终点。 |
| 注释 / 文档 | 源码注释英文；用户文档中文。标识符保持英文。 |
| 版权 | Copyright (c) 2026 The Mogu Authors. |

## 架构（`src/app/views` 组合 vs `ui::views` 工具箱）

```
SmartGisViews.exe                         唯一产品入口（终局）
  src/app/views                           只组合，不手绘业务面板
    ui::views::Widget                     单 HWND 窗口
      Splitter                            可改尺寸；非 dock / 非 MDI
        ├── AmboxView                     工具箱（原 Outlook AMBox）
        ├── CatalogView + LayerTree       数据源 / 图层
        ├── TabStrip
        │     ├── Map edit 2D
        │     │     MapViewport           子 HWND（不包装 CView）
        │     │       content::ViewHost   命令 / 输入 / 手势
        │     │       gis + render        像素仍在现有 C++ 栈
        │     ├── Datasource
        │     └── 3D                      同一 MapViewport / 3D 设备挂 HWND
        ├── AttributeTable / FeatureInfo
        └── StatusBar
      ChartView                           统计图（面板或 Dialog 模态）

src/ui/views                              公共工具箱 ui::views
  View / Widget / Splitter / Theme / primitives
  GIS widgets + AmboxView + ChartView + MapViewport

src/render/skia                           fill / text 画布（不是控件库）
src/content/public                        ViewHost / PluginHost / MapContents
src/legacy_ui/{gui,mfc_ex,xview,xcatalog, leftover（parity 前继续编 SmartGis.exe）
        xambox,chart} + src/legacy_app
```

Chrome 只 include `content/public`。插件贡献走 `content::PluginHost`；`AmboxView` 不链 leftover `SmtAModuleManager`。

## 文件所有权

| 树 | 职责 | 谁改 |
| --- | --- | --- |
| `src/ui/views/` | 工具箱内核、原语、GIS 面板、`AmboxView`、`ChartView`、`MapViewport` | toolkit / 本规格 widget |
| `src/app/views/` | 组合 `Widget` + `Splitter` + tabs；`SmartGisViews.exe` | 产品壳 agent |
| `src/content/public/` | `ViewHost`、`PluginHost`、`MapContents` | content / plugin |
| `src/render/skia/` | chrome 画布 | render |
| `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | leftover MFC；本轮不删 | 冻结（只修编译） |
| `src/legacy_app/`（`CMainFrame` / `CView`） | `SmartGis.exe` 直到 parity | 冻结 |

禁止：在 `src/app/views` 再写一套 catalog / ambox / chart 手绘 `View`；禁止 `src/chrome/`；禁止把 leftover `CView` 嵌进 Views。

## leftover MFC → Views 映射

| leftover 模块 / 类型 | 角色 | Views 类型 |
| --- | --- | --- |
| `src/app` `CMainFrame` / `CChildFrm` / MDI（现 `src/legacy_app`） | 主框 + 多文档 | 单 `Widget` + `Splitter`（非 MDI） |
| `CSmartMapEditView` / `Smt2DXView` / `Smt2DXEditView` | 2D 地图 `CView` | `MapViewport` + `content::ViewHost`；Tab「Map edit 2D」 |
| `CSmartDataSourceView` | 数据源页 | Tab「Datasource」+ `CatalogView` |
| `CSmart3DView` / `Smt3DXView` | 3D `CView` | Tab「3D」+ `MapViewport`（3D 设备仍挂 HWND） |
| `src/ui/xview` `SmtXView` 及派生 | 地图 `CView` / 监听 | **不**包装；输入改走 `ViewHost` |
| `src/ui/xcatalog` `SmtXCatalog` / `SmtDsXCatalog` / `SmtMapDocXCatalog` | 目录树 | `CatalogView` + `LayerTree` |
| `src/ui/xambox` `SmtAMBoxMgrDocBar` (`CBCGPOutlookBar`) / `SmtXAMBox` | Outlook 工具箱 | `AmboxView` |
| `src/ui/chart` `CDlg2DXChartView` / `SmtChart` / `SmtStaDiagram` | MFC 统计图 | `ChartView`（可选 `ChartView::run_modal`） |
| `src/legacy_ui/gui` `CDlg2DFeatureInfo` | 要素信息 | `FeatureInfo` |
| `src/legacy_ui/gui` 配置 / 输入对话框 | 杂项 dialog | `Dialog` + primitives（对等后） |
| `src/legacy_ui/mfc_ex` `CGridCtrl` | 属性表 | `AttributeTable` / `TableView` |
| `src/legacy_ui/mfc_ex` `CStackedWndDockBar` / `CTabbedWndDockBar` | dock 条 | `Splitter` + `TabStrip`（非真 dock） |
| `CBCGP*` / Feature Pack `CMFC*` | BCG 皮肤 | 不像素级复刻；Theme 深色 chrome |
| leftover `*.am` 菜单灌入 AMBox | 插件工具项 | `PluginHost` 贡献 → `AmboxView`（现无 list API 则 dummy 组） |

## MapViewport + ViewHost

地图不是 Skia 控件，也不是 `CView` 子类。

1. `src/app/views` 把 `MapViewport` 放进 Tab 页。
2. `MapViewport` 建子 HWND（`realize_native`），像素走已有 attach：`content::MapView` / OOP `SmartGisRender.exe` / `SmtRenderDevice::Init` / placeholder。
3. `content::ViewHost` 拥有 `EventBus`、`sdb::EditSession`、`tool::Workspace`；`execute` / `activate` / `dispatch_input` / `execute_legacy`。Chrome 把鼠标键盘交给 ViewHost，不调用 leftover `SmtXView::WindowProc`。

`MapViewport` 不 `Init` 渲染设备，除非走其 attach 路径。

## Splitter 布局

单窗口。`Splitter` 只做可拖拽分栏（左右目录/工具箱 vs 中央 tabs vs 可选底栏），**不是** BCG dock、**不是** MDI。

建议默认：

- 左：`AmboxView`（上）+ `CatalogView`（下）
- 中：`TabStrip` — Map edit 2D / Datasource / 3D
- 底：`StatusBar`；识别结果用 `FeatureInfo` / `AttributeTable`（可再分一栏）
- `ChartView`：统计命令弹出模态，或临时塞进 Datasource 页

窗口可改尺寸；分栏比例由 `Splitter` 保存于 Widget 生命周期内即可（本轮不要求持久化）。

## leftover 退役（parity 之后）

达到下列对等后，**停止编译** leftover MFC UI，仍先保留源码树（本轮不删）：

- `SmartGisViews.exe` 能完成：开图 / 图层开关 / 选择·漫游·识别 / 数据源浏览 / 3D 页挂 HWND / 工具箱点命令 / 简单统计图
- `views_unittests` 与 `SmartGisViews.exe --self-test` 绿
- 插件 UI 不再实例化 leftover `CDlg*`

然后：

1. `build.bat app` / `//:smartgis` 不再编 `SmartGis.exe`
2. `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` 与 `src/legacy_app` MFC 目标移出日常图
3. 源码删除另开变更（本规格不授权本轮删除）

## 测试

| 入口 | 作用 |
| --- | --- |
| `views_unittests` | 工具箱内核 / 原语 / GIS 面板 / `AmboxView` / `ChartView`（无 MFC） |
| `SmartGisViews.exe --self-test` | 产品壳 e2e：Splitter + tabs + `MapViewport` 挂 HWND |

不要把 leftover `SmartGis.exe` 当终局冒烟。

## 范围外

- 不 vendor Chromium / Aura / Blink / 完整 Skia 树
- 禁止 Qt（Widgets / Quick / QML / 任何 Qt 模块）
- 不像素级复刻 BCG / Feature Pack
- 本轮不删除 leftover MFC 源码
- 不把 WinUI / WebView2 / Feature Pack 升为终局工具箱
- 不把 Views 目标塞进 `//src:src_all`

---

**最后更新：** 2026-09-13
