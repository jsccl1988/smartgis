<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` 子目录职责切分 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development（推荐并行）或 executing-plans。规格：[`../specs/2026-09-19-ui-views-subdir-responsibility-design.md`](../specs/2026-09-19-ui-views-subdir-responsibility-design.md)。**不要 commit**，除非用户明确要求。工作在 **master**。多 agent 时严格按轨道分区，**D 独占** `BUILD.gn` / `views.h` / `views.cc` / 活跃文档。

**Goal:** 按职责把 `src/ui/views` 切成 `kernel` / `primitives` / `dialogs` / `gis` / `map` / `testing`，include 干净打断，单 GN 目标，可并行落地。

**Architecture:** 头源同职责目录；命名空间仍 `ui::views`；调用方改 `"ui/views/<area>/…"`；无根转发头。

**Tech Stack:** GN/Ninja、`build.bat`、`git mv`、PowerShell 批量替换 include。

## Global Constraints

- 命名空间：仅 `ui::views`（不引入 `ui::views::kernel`）
- Include：`"ui/views/<area>/foo.h"`；无 shim
- GN：一个 `source_set("views")`
- 分支：只在 `master` 改
- 注释：新增/改动用英文；对话/本 plan 中文
- 禁止：Qt、多 DLL 拆分、行为重写

## Include 映射表（权威）

| 旧 `#include "ui/views/…"` | 新路径 |
| --- | --- |
| `view.h` `widget.h` `layout.h` `layout_check.h` `theme.h` `event.h` `dpi.h` `splitter.h` `dialog_host.h` | `kernel/…` |
| `button.h` `label.h` `textfield.h` `checkbox.h` `radio_button.h` `combobox.h` `tab_strip.h` `table_view.h` `tree_view.h` `scroll_view.h` `menu_bar.h` `context_menu.h` | `primitives/…` |
| `dialog.h` `file_picker.h` `message_box.h` `input_text_dialog.h` `select_one_dialog.h` `create_datasource_dialog.h` `create_layer_dialog.h` `create_map_dialog.h` `att_struct_dialog.h` `add_basemap_dialog.h` | `dialogs/…` |
| `catalog_view.h` `layer_tree.h` `attribute_table.h` `feature_info.h` `status_bar.h` `ambox_view.h` `chart_view.h` | `gis/…` |
| `map_viewport.h` `touch_multitouch.h` | `map/…` |

---

### Task 0: 冻结目录 + 建空目录（协调者，串行）

**Files:**
- Create dirs: `src/ui/views/dialogs/`, `src/ui/views/map/`, `src/ui/views/testing/`
- `kernel/` `primitives/` `gis/` 已存在

- [ ] **Step 1:** 确认规格已读；打印映射表给并行 agent
- [ ] **Step 2:** `mkdir` 三个新目录（若尚无）
- [ ] **Step 3:** 在共享频道/本 plan 标注「路径冻结」后再开 A

---

### Task A: kernel + primitives（头源同目录）

**Files:**
- Move headers from `src/ui/views/*.h` → `kernel/` 或 `primitives/`（见映射表）
- Existing: `src/ui/views/kernel/*.cc`, `src/ui/views/primitives/*.cc`（对话框 `.cc` **本任务勿动**，留给 B）
- Update includes **仅**在已迁入 `kernel/` + `primitives/` 的文件内

**Produces:** `"ui/views/kernel/view.h"` 等路径可用；`primitives` 内无 dialog 头

- [ ] **Step 1:** `git mv` kernel 头进 `kernel/`：

```bat
git mv src/ui/views/view.h src/ui/views/kernel/view.h
git mv src/ui/views/widget.h src/ui/views/kernel/widget.h
git mv src/ui/views/layout.h src/ui/views/kernel/layout.h
git mv src/ui/views/layout_check.h src/ui/views/kernel/layout_check.h
git mv src/ui/views/theme.h src/ui/views/kernel/theme.h
git mv src/ui/views/event.h src/ui/views/kernel/event.h
git mv src/ui/views/dpi.h src/ui/views/kernel/dpi.h
git mv src/ui/views/splitter.h src/ui/views/kernel/splitter.h
git mv src/ui/views/dialog_host.h src/ui/views/kernel/dialog_host.h
```

- [ ] **Step 2:** `git mv` primitives 头（**排除** dialogs）进 `primitives/`：

```bat
git mv src/ui/views/button.h src/ui/views/primitives/button.h
git mv src/ui/views/label.h src/ui/views/primitives/label.h
git mv src/ui/views/textfield.h src/ui/views/primitives/textfield.h
git mv src/ui/views/checkbox.h src/ui/views/primitives/checkbox.h
git mv src/ui/views/radio_button.h src/ui/views/primitives/radio_button.h
git mv src/ui/views/combobox.h src/ui/views/primitives/combobox.h
git mv src/ui/views/tab_strip.h src/ui/views/primitives/tab_strip.h
git mv src/ui/views/table_view.h src/ui/views/primitives/table_view.h
git mv src/ui/views/tree_view.h src/ui/views/primitives/tree_view.h
git mv src/ui/views/scroll_view.h src/ui/views/primitives/scroll_view.h
git mv src/ui/views/menu_bar.h src/ui/views/primitives/menu_bar.h
git mv src/ui/views/context_menu.h src/ui/views/primitives/context_menu.h
```

- [ ] **Step 3:** 更新 `kernel/` + `primitives/` 内所有 `#include "ui/views/<stem>.h"` 为映射表新路径；include guard 改为 `UI_VIEWS_KERNEL_*` / `UI_VIEWS_PRIMITIVES_*`

- [ ] **Step 4:** 自检：`rg "ui/views/(view|widget|button|label)\\.h" src/ui/views/kernel src/ui/views/primitives` 应无旧路径（本区文件内）

- [ ] **Step 5:** **不要**改 `BUILD.gn` / `views.h` / `src/app`（归 D）；本轨完成后通知「A 完成」

---

### Task B: dialogs（与 C 并行）

**Files:**
- Move: 根上对话框头 → `dialogs/`
- Move: `primitives/{dialog,file_picker,message_box,input_text_dialog,select_one_dialog}.cc` → `dialogs/`
- Move: `gis/{create_*,att_struct_dialog,add_basemap_dialog}.cc` → `dialogs/`
- Update includes **仅**在 `dialogs/` 内文件

**Produces:** 全部对话框在 `dialogs/`；`primitives/` 与 `gis/` 无 dialog 实现

- [ ] **Step 1:** `git mv` 头：

```bat
git mv src/ui/views/dialog.h src/ui/views/dialogs/dialog.h
git mv src/ui/views/file_picker.h src/ui/views/dialogs/file_picker.h
git mv src/ui/views/message_box.h src/ui/views/dialogs/message_box.h
git mv src/ui/views/input_text_dialog.h src/ui/views/dialogs/input_text_dialog.h
git mv src/ui/views/select_one_dialog.h src/ui/views/dialogs/select_one_dialog.h
git mv src/ui/views/create_datasource_dialog.h src/ui/views/dialogs/create_datasource_dialog.h
git mv src/ui/views/create_layer_dialog.h src/ui/views/dialogs/create_layer_dialog.h
git mv src/ui/views/create_map_dialog.h src/ui/views/dialogs/create_map_dialog.h
git mv src/ui/views/att_struct_dialog.h src/ui/views/dialogs/att_struct_dialog.h
git mv src/ui/views/add_basemap_dialog.h src/ui/views/dialogs/add_basemap_dialog.h
```

- [ ] **Step 2:** `git mv` 源：

```bat
git mv src/ui/views/primitives/dialog.cc src/ui/views/dialogs/dialog.cc
git mv src/ui/views/primitives/file_picker.cc src/ui/views/dialogs/file_picker.cc
git mv src/ui/views/primitives/message_box.cc src/ui/views/dialogs/message_box.cc
git mv src/ui/views/primitives/input_text_dialog.cc src/ui/views/dialogs/input_text_dialog.cc
git mv src/ui/views/primitives/select_one_dialog.cc src/ui/views/dialogs/select_one_dialog.cc
git mv src/ui/views/gis/create_datasource_dialog.cc src/ui/views/dialogs/create_datasource_dialog.cc
git mv src/ui/views/gis/create_layer_dialog.cc src/ui/views/dialogs/create_layer_dialog.cc
git mv src/ui/views/gis/create_map_dialog.cc src/ui/views/dialogs/create_map_dialog.cc
git mv src/ui/views/gis/att_struct_dialog.cc src/ui/views/dialogs/att_struct_dialog.cc
git mv src/ui/views/gis/add_basemap_dialog.cc src/ui/views/dialogs/add_basemap_dialog.cc
```

- [ ] **Step 3:** 更新 `dialogs/` 内 include → `kernel/` / `primitives/` / `dialogs/` 新路径；guard → `UI_VIEWS_DIALOGS_*`

- [ ] **Step 4:** 通知「B 完成」；勿碰 `BUILD.gn`

---

### Task C: gis 面板头 + map hang（与 B 并行）

**Files:**
- Move: 根上 GIS 面板头 → `gis/`
- Move: `gis/map_viewport.cc` `gis/touch_multitouch.cc` + 根头 → `map/`
- Update includes **仅**在 `gis/` + `map/` 内

**Produces:** `gis/` 无 hang；`map/` 自洽

- [ ] **Step 1:** `git mv` 面板头进 `gis/`：

```bat
git mv src/ui/views/catalog_view.h src/ui/views/gis/catalog_view.h
git mv src/ui/views/layer_tree.h src/ui/views/gis/layer_tree.h
git mv src/ui/views/attribute_table.h src/ui/views/gis/attribute_table.h
git mv src/ui/views/feature_info.h src/ui/views/gis/feature_info.h
git mv src/ui/views/status_bar.h src/ui/views/gis/status_bar.h
git mv src/ui/views/ambox_view.h src/ui/views/gis/ambox_view.h
git mv src/ui/views/chart_view.h src/ui/views/gis/chart_view.h
```

- [ ] **Step 2:** `git mv` map hang：

```bat
git mv src/ui/views/map_viewport.h src/ui/views/map/map_viewport.h
git mv src/ui/views/touch_multitouch.h src/ui/views/map/touch_multitouch.h
git mv src/ui/views/gis/map_viewport.cc src/ui/views/map/map_viewport.cc
git mv src/ui/views/gis/touch_multitouch.cc src/ui/views/map/touch_multitouch.cc
```

- [ ] **Step 3:** 更新 `gis/` + `map/` 内 include；`map` **不得** include `gis/*` 面板；guard → `UI_VIEWS_GIS_*` / `UI_VIEWS_MAP_*`

- [ ] **Step 4:** 通知「C 完成」

---

### Task D: testing + GN + 伞头 + 调用方 + 文档（扫尾）

**Files:**
- Move: `pixel_harness.*` `pixel_png_wic.cc` `views_unittests.cc` `views_pixel_tests.cc` `testdata/` → `testing/`
- Modify: `src/ui/views/BUILD.gn`（全部新路径）
- Modify: `src/ui/views/views.h` / `views.cc`
- Modify: `src/app/views/**` 及任何仍引用旧 include 的树内文件
- Modify docs: `src/ui/views/README.md`, `docs/build/ui-views-skia.md`, `docs/build/ui-testing.md`, `docs/build/src-layout.md`（示例路径）, `docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`（nesting 段标修订）

**Consumes:** A/B/C 完成且路径冻结

- [ ] **Step 1:** `git mv` 测试资产进 `testing/`：

```bat
git mv src/ui/views/pixel_harness.h src/ui/views/testing/pixel_harness.h
git mv src/ui/views/pixel_harness.cc src/ui/views/testing/pixel_harness.cc
git mv src/ui/views/pixel_png_wic.cc src/ui/views/testing/pixel_png_wic.cc
git mv src/ui/views/views_unittests.cc src/ui/views/testing/views_unittests.cc
git mv src/ui/views/views_pixel_tests.cc src/ui/views/testing/views_pixel_tests.cc
git mv src/ui/views/testdata src/ui/views/testing/testdata
```

- [ ] **Step 2:** 修正 harness / pixel tests 内 golden 相对路径（若原先相对 `src/ui/views/testdata`，改为相对 `testing/testdata` 或 `__FILE__` 旁路径）

- [ ] **Step 3:** 重写 `BUILD.gn` `sources`：头与 `.cc` 全部带 `kernel/` `primitives/` `dialogs/` `gis/` `map/`；测试 target 指向 `testing/…`

- [ ] **Step 4:** 重写 `views.h` 为新 include 列表（完整枚举规格归属表）

- [ ] **Step 5:** 全仓替换旧 include。PowerShell 示例（在仓库根）：

```powershell
$map = @{
  'ui/views/view.h' = 'ui/views/kernel/view.h'
  'ui/views/widget.h' = 'ui/views/kernel/widget.h'
  'ui/views/layout.h' = 'ui/views/kernel/layout.h'
  'ui/views/layout_check.h' = 'ui/views/kernel/layout_check.h'
  'ui/views/theme.h' = 'ui/views/kernel/theme.h'
  'ui/views/event.h' = 'ui/views/kernel/event.h'
  'ui/views/dpi.h' = 'ui/views/kernel/dpi.h'
  'ui/views/splitter.h' = 'ui/views/kernel/splitter.h'
  'ui/views/dialog_host.h' = 'ui/views/kernel/dialog_host.h'
  'ui/views/button.h' = 'ui/views/primitives/button.h'
  'ui/views/label.h' = 'ui/views/primitives/label.h'
  'ui/views/textfield.h' = 'ui/views/primitives/textfield.h'
  'ui/views/checkbox.h' = 'ui/views/primitives/checkbox.h'
  'ui/views/radio_button.h' = 'ui/views/primitives/radio_button.h'
  'ui/views/combobox.h' = 'ui/views/primitives/combobox.h'
  'ui/views/tab_strip.h' = 'ui/views/primitives/tab_strip.h'
  'ui/views/table_view.h' = 'ui/views/primitives/table_view.h'
  'ui/views/tree_view.h' = 'ui/views/primitives/tree_view.h'
  'ui/views/scroll_view.h' = 'ui/views/primitives/scroll_view.h'
  'ui/views/menu_bar.h' = 'ui/views/primitives/menu_bar.h'
  'ui/views/context_menu.h' = 'ui/views/primitives/context_menu.h'
  'ui/views/dialog.h' = 'ui/views/dialogs/dialog.h'
  'ui/views/file_picker.h' = 'ui/views/dialogs/file_picker.h'
  'ui/views/message_box.h' = 'ui/views/dialogs/message_box.h'
  'ui/views/input_text_dialog.h' = 'ui/views/dialogs/input_text_dialog.h'
  'ui/views/select_one_dialog.h' = 'ui/views/dialogs/select_one_dialog.h'
  'ui/views/create_datasource_dialog.h' = 'ui/views/dialogs/create_datasource_dialog.h'
  'ui/views/create_layer_dialog.h' = 'ui/views/dialogs/create_layer_dialog.h'
  'ui/views/create_map_dialog.h' = 'ui/views/dialogs/create_map_dialog.h'
  'ui/views/att_struct_dialog.h' = 'ui/views/dialogs/att_struct_dialog.h'
  'ui/views/add_basemap_dialog.h' = 'ui/views/dialogs/add_basemap_dialog.h'
  'ui/views/catalog_view.h' = 'ui/views/gis/catalog_view.h'
  'ui/views/layer_tree.h' = 'ui/views/gis/layer_tree.h'
  'ui/views/attribute_table.h' = 'ui/views/gis/attribute_table.h'
  'ui/views/feature_info.h' = 'ui/views/gis/feature_info.h'
  'ui/views/status_bar.h' = 'ui/views/gis/status_bar.h'
  'ui/views/ambox_view.h' = 'ui/views/gis/ambox_view.h'
  'ui/views/chart_view.h' = 'ui/views/gis/chart_view.h'
  'ui/views/map_viewport.h' = 'ui/views/map/map_viewport.h'
  'ui/views/touch_multitouch.h' = 'ui/views/map/touch_multitouch.h'
}
# Prefer longest keys first (dialog_host before dialog) and UTF-8 write.
Get-ChildItem -Recurse -Include *.h,*.cc,*.cpp,*.md -Path src,docs |
  Where-Object { $_.FullName -notmatch '\\out\\' } |
  ForEach-Object {
    $c = Get-Content -Raw $_.FullName
    $n = $c
    foreach ($k in ($map.Keys | Sort-Object Length -Descending)) {
      $n = $n.Replace($k, $map[$k])
    }
    if ($n -ne $c) { Set-Content -NoNewline -Path $_.FullName -Value $n }
  }
```

- [ ] **Step 6:** 更新 README + `ui-views-skia.md` nesting 段 + controls 设计 Status 注「nesting 被 2026-09-19 设计取代」+ `ui-testing.md` / `src-layout.md` 示例路径

- [ ] **Step 7:** 验收：

```bat
rg "#include \"ui/views/[a-z0-9_]+\.h\"" src --glob "!**/views.h"
```

期望：无匹配（或仅误报）。根目录：

```bat
dir src\ui\views\*.h
```

期望：仅 `views.h`。

- [ ] **Step 8:** 编译（用户/环境允许时）：

```bat
build.bat views
```

并跑 `views_unittests` / `views_pixel_tests`（以 `build.bat` / GN 既有入口为准）。

---

## 并行调度

```
Task0 (freeze)
   │
   ▼
 Task A
   │
   ├──────────────┐
   ▼              ▼
 Task B        Task C
   │              │
   └──────┬───────┘
          ▼
       Task D
```

多 agent：A 一人；B/C 两人并行；D 一人（或协调者）。冲突文件：`BUILD.gn`、`views.h`、`README.md`、文档 — **仅 D 写**。

## Spec coverage（自检）

| 规格项 | 任务 |
| --- | --- |
| 六目录终态 | A–D |
| 干净打断 include | A–C 区内 + D 全仓 |
| 对话框全进 dialogs | B |
| gis 无 hang | C |
| testing 迁出 | D |
| nesting 文档修订 | D |
| 单 GN views | D |
| 并行约束 | 本 plan 调度段 |
