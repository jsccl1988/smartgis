<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Leftover chrome 整包对等（组合优先）

**Date:** 2026-09-14  
**Status:** accepted  
**Scope:** 在已锁定的 Views + Skia 终态上，把 leftover `src/ui/{xview,xcatalog,xambox,chart,gui,mfc_ex}` 的用户可见能力做到 `SmartGisViews.exe` 可演示对等。架构权威仍见 [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md) 与 [`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)。本文件只锁定 **缺口、接线与验收**。

## 目标

组合优先：不原地升级 leftover MFC 树。壳布局已基本在 `app::BrowserView`；本包补 **数据/事件/命令闭环**，使开图、图层、选择/识别、工具箱、属性检视、统计图、杂项 Dialog 在 Views 路径可用。

## 锁定决策

| 主题 | 选择 |
| --- | --- |
| 落地方式 | 组合优先（方案 1） |
| leftover 树 | 冻结；只修编译；本轮不删 |
| 产品入口 | `SmartGisViews.exe` |
| 地图 | `MapViewport` + `content::ViewHost`；不包装 `CView` |
| 控件持有 GIS | 禁止；只传 string / opaque id |
| UI 工具箱 | 禁止 Qt；禁止真 dock / MDI |
| 提交 | 仅用户明确要求时才 `git commit` |

## 架构边界

```
app::BrowserView                 组合 + 订阅 EventBus + 调 Dialog/Chart
  CatalogView / LayerTree        ← xcatalog
  MapViewport ×3 + ViewHost      ← xview
  AmboxView                      ← xambox（可枚举则从 PluginHost/CommandCatalog）
  FeatureInfo / AttributeTable   ← gui FeatureInfo / mfc_ex Grid
  ChartView::run_modal           ← chart
  *Dialog                        ← gui 杂项（已有）

content::EventBus                SelectionChanged / ExtentChanged
tool::CommandCatalog             可枚举 id（本包补 list）
leftover src/ui/{…}              不动功能
```

## 模块映射与本包缺口

| leftover | Views 目标 | 壳现状 | 本包要做 |
| --- | --- | --- | --- |
| xview | MapViewport + ViewHost | 已挂接 | 保持；输入继续 `dispatch_input` |
| xcatalog | CatalogView + LayerTree | 已组合；多为 demo 数据 | 开图后尽量 `CatalogCall` 驱动；无图例 API 时保留 demo + 状态栏反馈 |
| xambox | AmboxView | 已组合；dummy 三组 | `CommandCatalog` 可枚举时填工具组；否则保留 Select/Pan/Identify |
| chart | ChartView | 未进菜单 | Menu「Chart」→ `run_modal`（系列来自图层可见计数或固定 demo） |
| gui FeatureInfo | FeatureInfo | 已进 inspector Tab | 订阅 `SelectionChanged` → `set_feature_id` / `set_fields` |
| gui Dialogs | InputText / AttStruct / Create* | 已接 catalog 命令 | 保持；补单测覆盖入口即可 |
| mfc_ex Grid | AttributeTable | 已进 Tab | 同选择事件写一行或多行字符串表；不做原地单元格编辑 |

## 数据流

1. **选择 / 识别**  
   `Workspace` 在 `select.*` 草稿上已 `publish(SelectionChanged)`。  
   `BrowserView` 对每个 `ViewHost::events()` 订阅：  
   - 空 `ids` → `FeatureInfo::clear()`，AttributeTable 可清空或保留上次  
   - 非空 → FeatureInfo 显示 token（FeatureId 十六进制）+ 字段 `id`/`view_id`；AttributeTable 设列 `id` + 一行  

2. **工具箱**  
   `AmboxView::populate_from_plugin_host`：若 `commands()` 可枚举，按 id 前缀分组（`selection.*`→Select，`view.*`→Pan/View，其余→Tools）；否则 dummy 三组。点击仍 `ViewHost::activate` / `execute`。

3. **统计图**  
   MenuBar「Chart」调用 `ChartView::run_modal`。系列：优先用 LayerTree 可见层名=1.0；无层则用内置三点 demo。

4. **Catalog**  
   现有 `CatalogCall` JSON 路径保持。本包不发明第二套图例协议；`LegendSnapshot` 仍可为空实现。

## 错误与降级

- 无 `MapContents` / 无 host：状态栏提示；控件不崩  
- `PluginHost` 空或无 list：Ambox dummy 组  
- `SelectionChanged` 无属性载荷：只显示 opaque id，不拉 `SmtFeature*`  
- Chart Dialog 不可用（无 dialog.h）：菜单项 no-op 或状态栏说明  

## 验收（parity）

- `SmartGisViews.exe`：Open；Catalog 命令弹 Dialog；Ambox Select/Pan/Identify；选择后 FeatureInfo/AttributeTable 更新；Chart 模态可出  
- `views_unittests` 绿（含 Ambox 枚举路径、Chart 系列、FeatureInfo/AttributeTable 字符串填充）  
- `--self-test` 绿  
- 不修改 leftover `src/ui/{xview,xcatalog,xambox,chart,gui,mfc_ex}` 功能代码（编译修复除外）

## 范围外

- 像素级 BCG / Feature Pack  
- Grid 原地编辑、真 dock、MDI  
- Vendor Chromium / 完整 Skia  
- 删除 leftover 源码  
- 把 Views 塞进 `src_all`

---

**最后更新：** 2026-09-14
