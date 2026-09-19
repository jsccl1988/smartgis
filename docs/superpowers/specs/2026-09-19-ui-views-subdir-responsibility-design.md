<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` 子目录职责切分（设计）

Status: active  
Date: 2026-09-19  
Plan: [`../plans/2026-09-19-ui-views-subdir-responsibility.md`](../plans/2026-09-19-ui-views-subdir-responsibility.md)

## 背景

`src/ui/views` 已按「根公开头 + `kernel/` / `primitives/` / `gis/` 只放 `.cc`」落地，但职责仍混杂：

- 通用对话框与 Button/Label 同捆在 `primitives/`
- GIS 面板、GIS 业务对话框、`MapViewport` HWND hang 同捆在 `gis/`
- 像素 harness / goldens / 单测 TU 与 toolkit 源码同级

用户选定：**按职责全切（对话框 / 面板 vs map hang / 测试迁出）+ include 干净打断（无根转发头）+ 单 GN 目标 + 并行落地**。

## 决策（锁定）

| # | 决策 | 终态 |
| --- | --- | --- |
| 1 | 公开 nest | 允许 `src/ui/views/{kernel,primitives,dialogs,gis,map,testing}/` 作为公开 include 路径 |
| 2 | Include | `"ui/views/<area>/foo.h"`；**无**根目录业务头、**无**转发 shim |
| 3 | 命名空间 | 仍两层 `ui::views`；目录不升第三语义层 |
| 4 | GN | 仍一个 `source_set("views")`；测试 target 的 `sources` 指向 `testing/` |
| 5 | 对话框 | **全部**进 `dialogs/`（通用 + GIS 业务） |
| 6 | GIS | 仅壳面板；不含 map hang、不含对话框 |
| 7 | Map | `map/` 仅 `MapViewport` + `TouchMultitouch` |
| 8 | 测试 | harness / `views_*tests` / `testdata/` → `testing/` |
| 9 | 并行 | 轨道 A→(B∥C)→D；共享文件（`BUILD.gn` / `views.h` / 文档）由 D 或协调者独占 |

## 终态树

```
src/ui/views/
  BUILD.gn
  README.md
  views.h / views.cc
  kernel/       View Widget Layout Theme Event Dpi Splitter DialogHost LayoutCheck
  primitives/   Button Label Textfield Checkbox Radio Combobox
                TabStrip TableView TreeView ScrollView MenuBar ContextMenu
  dialogs/      Dialog FilePicker MessageBox InputText SelectOne
                CreateDatasource/Layer/Map AttStruct AddBasemap
  gis/          Catalog LayerTree AttributeTable FeatureInfo StatusBar Ambox Chart
  map/          MapViewport TouchMultitouch
  testing/      pixel_harness pixel_png_wic views_unittests views_pixel_tests testdata/
```

禁止更深公开 nest（例如 `gis/panels/`、`dialogs/gis/`）。

## 依赖边界（逻辑，非多 target）

```
kernel ← primitives ← dialogs
                    ← gis
         map 依赖 kernel（+ content/view_host；不依赖 gis 面板）
testing 可依赖上述全部；产品库不反向依赖 testing
```

本轮不拆多个 `source_set`。GIS 对话框默认只依赖 `primitives`/`kernel`；确需面板类型时再 include `gis/`。

## Nesting-cap 修订

既有文案（`docs/build/ui-views-skia.md`、`src/ui/views/README.md`、`2026-09-13-ui-views-controls-design.md`）写「公开头必须在根、子目录仅 `.cc`」。本设计**取代**该约定：

- 模块 nest 仍是 `src/ui/views`（一层 module）
- 其下六个职责目录是**公开实现分区**，不是第三层 `src/ui/views/widget/` 式的随意 nest
- 调用方 include 必须带职责段：`"ui/views/kernel/view.h"`

活跃文档就地改写；archive / 历史 plan 可不改。

## 迁移规则

1. `git mv` 头文件进入对应职责目录；已有同名 `.cc` 一并迁入（`dialogs` / `map` 从 `primitives`/`gis` 抽出）。
2. 全仓替换 `#include "ui/views/X.h"` → `#include "ui/views/<area>/X.h"`。
3. Include guard 建议改为 `UI_VIEWS_<AREA>_X_H_`（与路径一致）。
4. 根目录仅保留：`BUILD.gn`、`README.md`、`views.h`、`views.cc`。
5. `views.h` 伞头改为列举新路径；`module_id()` 等行为不变。
6. 像素测试：golden 路径相对 `testing/testdata/`（或 harness 内用相对本文件的路径）保持可解析。

## 文件归属表

| 区域 | 文件（茎名） |
| --- | --- |
| kernel | `view` `widget` `layout` `layout_check` `theme` `event` `dpi` `splitter` `dialog_host` |
| primitives | `button` `label` `textfield` `checkbox` `radio_button` `combobox` `tab_strip` `table_view` `tree_view` `scroll_view` `menu_bar` `context_menu` |
| dialogs | `dialog` `file_picker` `message_box` `input_text_dialog` `select_one_dialog` `create_datasource_dialog` `create_layer_dialog` `create_map_dialog` `att_struct_dialog` `add_basemap_dialog` |
| gis | `catalog_view` `layer_tree` `attribute_table` `feature_info` `status_bar` `ambox_view` `chart_view` |
| map | `map_viewport` `touch_multitouch` |
| testing | `pixel_harness` `pixel_png_wic` `views_unittests` `views_pixel_tests` `testdata/` |

## 并行轨道

| 轨道 | 范围 | 并行约束 |
| --- | --- | --- |
| **A** | `kernel` + `primitives` 头源同目录 + 区内 include | 先合；冻结路径名 |
| **B** | `dialogs` 抽出（含 GIS 对话框） | 与 C 并行；不改 A 已合文件 |
| **C** | `gis` 面板头迁入；`map/` 从 gis 抽出 hang | 与 B 并行 |
| **D** | `testing/`、调用方（`src/app/views` 等）、`BUILD.gn`/`views.h`、活跃文档 | 目录名冻结后扫尾；独占共享文件 |

## 非目标

- 不拆多个 GN `source_set` / DLL
- 不改控件行为、不引入 Qt、不 vendor Chromium/Skia
- 不借机重写 MFC 迁移范围
- 不自动重建 CBM 索引

## 验收

- 根下无业务 `*.h`（仅伞头与构建/说明）
- 树内无旧路径 `#include "ui/views/<stem>.h"`（伞头除外的 stem 列表）
- `views_unittests` / `views_pixel_tests` 源与 goldens 在 `testing/`
- 关键：`build.bat` 构建 `views` 及相关测试（环境允许时）

## 相关

- As-built：[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)
- Controls：[`2026-09-13-ui-views-controls-design.md`](2026-09-13-ui-views-controls-design.md)（本设计修订其 nesting 段）
- 模块 README：[`src/ui/views/README.md`](../../../src/ui/views/README.md)
