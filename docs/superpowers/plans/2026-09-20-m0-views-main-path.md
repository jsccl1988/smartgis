<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# M0 — Views 主路径可干活 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 仅用 `SmartGisViews.exe` 闭环 **打开 GPKG → 平移缩放 → FeatureInfo → 追加折线并写回文件**；`build.bat e2e`（含 Views `--self-test`）绿。日常产品入口保持 `build.bat app` / `views`，不依赖 MFC。

**Architecture:** 不重开双栈大迁移。M0 在已有 `BrowserView` + `MapScene` + `content::ViewHost` + `tool::Workspace` 上补齐 **验收缺口**：`MapScene::write_path`（OGR 写出）、菜单/自测走 `edit.append.linestring`、选择后 FeatureInfo 可断言、自测 mark 覆盖差距矩阵 §6 M0 口令。SP5 闸门与 SP1b 指针搬迁视为 **前置已满足**（见下方），本 plan 不重做。

**Tech Stack:** C++23、GDAL/OGR（现有 `//third_party:gdal`）、`ui::views`、`content::ViewHost`、GN/`build.bat views` / `e2e`、gtest-free `*_test` main。

## Global Constraints

- Git：在 **master** 改；**不**新建分支；**不** `git commit`（除非用户明确要求）。
- 禁止 Qt；终局禁止 `#include "legacy/…"`；不碰 `src/legacy/**` 业务（编译修复除外）。
- 新函数 `snake_case`；公开命名空间 ≤ 两层；源码注释英文；用户文档中文。
- 版权：`Copyright (c) 2026 The Mogu Authors.`
- 控件只持 string / opaque id，**禁止** `SmtFeature*`。
- 「保存」= `MapScene::write_path` 写出 **GeoJSON**（单文件多 FeatureCollection 不硬撑；**默认一图层一文件**，多图层时写目录旁 `{stem}_{layer}.geojson` 或单层 active）。M0 **不**做完整 GPKG 多图层原地更新。
- 不吞并 M1（Style/瓦片深度）、M2 Processing、M3 流式 3D。

**Living gap matrix:** [`../../build/industry-gap-matrix.md`](../../build/industry-gap-matrix.md) §1.3 / §6 M0  
**Related (do not re-implement):**

| 前置 | 状态（2026-09-20） | 文档 |
| --- | --- | --- |
| Shell 编译闸门 SP5 | Success criteria 已勾 | [`../specs/2026-09-19-shell-compile-gate-design.md`](../specs/2026-09-19-shell-compile-gate-design.md) |
| Tool 行为 SP1b | Tasks 大体 landed | [`../plans/2026-09-19-tool-behavior-migration.md`](../plans/2026-09-19-tool-behavior-migration.md) |
| Chrome 组合对等 | accepted | [`../specs/2026-09-14-ui-leftover-chrome-parity-design.md`](../specs/2026-09-14-ui-leftover-chrome-parity-design.md) |
| Views 迁移架构 | active | [`../specs/2026-09-13-ui-views-mfc-migration-design.md`](../specs/2026-09-13-ui-views-mfc-migration-design.md) |
| UI 测试分层 | as-built | [`../../build/ui-testing.md`](../../build/ui-testing.md) |

---

## File map

| File | Role |
| --- | --- |
| `src/app/views/map_scene.h` `.cc` | `write_path`；可选 `last_write_path_` |
| `src/app/views/map_scene_test.cc` | open → append → write → reopen 往返 |
| `src/app/views/browser_view.h` `.cc` | `feature_info()` 访问器；Save / Draw Line 菜单；`catalog.map.save` → `write_path` |
| `src/app/views/main.cc` | `--self-test` M0 段：line append + FeatureInfo + write roundtrip marks |
| `docs/build/ui-testing.md` | 新退出码 / marks |
| `docs/build/industry-gap-matrix.md` | M0 行链到本 plan；完成后改成熟度 |
| `src/app/views/README.md` | Open / Draw line / Save 操作说明一句 |

---

### Task 1: `MapScene::write_path` + 单测往返

**Files:**
- Modify: `src/app/views/map_scene.h`、`src/app/views/map_scene.cc`
- Modify: `src/app/views/map_scene_test.cc`
- Consumes: existing `ingest_ogr_path` / `Layer` / `Feature` / `GeomKind`
- Produces: `bool write_path(const std::string& path) const;` — 将 **当前 active 可见层**（若无 active 则第一层可见）写成 GeoJSON；失败返回 false

- [ ] **Step 1: 写失败测试** — 在 `map_scene_test.cc` 追加用例：构造内存场景（或 `open_path` 小 geojson），`append_from_draft` 一条线（可手填 `Feature`），`write_path(tmp)`，再 `MapScene b; b.open_path(tmp)`，断言 `feature_count() >= 1` 且存在 `kLine`。

```cpp
// map_scene_test.cc — add near end of main(), before return g_fails
{
  char tmp[MAX_PATH] = {};
  // Prefer GetTempPathA + unique name, e.g. map_scene_m0_write.json
  DWORD n = GetTempPathA(MAX_PATH, tmp);
  expect(n > 0 && n < MAX_PATH, "temp path");
  std::string out = std::string(tmp) + "map_scene_m0_write.geojson";
  DeleteFileA(out.c_str());

  app::MapScene a;
  expect(a.create_layer("edit_line", "LineString"), "create line layer");
  tool::Draft draft{};
  // Draft must carry at least two map-space points; match append_from_draft
  // expectations used elsewhere (see map_scene.cc draw.line branch).
  // If Draft construction is awkward in-test, push a Feature manually via
  // a test-only friend OR call append_from_draft after execute path.
  // Preferred: build Draft with points in map lon/lat:
  draft.points = {{100.0, 30.0}, {110.0, 35.0}};
  const content::FeatureId id =
      a.append_from_draft(draft, "draw.linestring");
  expect(id.value != 0, "append line id");
  expect(a.write_path(out), "write_path");
  expect(GetFileAttributesA(out.c_str()) != INVALID_FILE_ATTRIBUTES,
         "file exists");

  app::MapScene b;
  expect(b.open_path(out), "reopen written");
  expect(b.last_open_was_ogr(), "reopen via OGR");
  expect(b.feature_count() >= 1, "reopen feature");
  DeleteFileA(out.c_str());
}
```

> 若 `Draft::points` 字段名不同，以 `tool/gestures.h` 实际成员为准（`vertices` / `points`）；实现前用 CBM/`get_code_snippet` 核对，**不要臆造字段**。

- [ ] **Step 2: 跑测确认失败**

```bat
build.bat map_scene_test
out\map_scene_test.exe
```

Expected: FAIL — `write_path` 未声明 / 链接失败，或断言 `write_path` false。

- [ ] **Step 3: 实现 `write_path`**

在 `map_scene.h` 公共区（`open_path` 旁）声明：

```cpp
  // Write the active visible layer to |path| as GeoJSON (OGR "GeoJSON" driver).
  // Returns false if no features, driver missing, or Create failed.
  bool write_path(const std::string& path) const;
```

在 `map_scene.cc`（`open_path` 后）实现要点：

1. `GDALAllRegister()`。
2. 取 active 层；若不可见或空，扫第一层 `visible && !features.empty()`。
3. `GetGDALDriverManager()->GetDriverByName("GeoJSON")`；`driver->Create(path, 0, 0, 0, GDT_Unknown, nullptr)`。
4. `CreateLayer`：按层内主导 `GeomKind` 选 `wkbPoint` / `wkbLineString` / `wkbPolygon`（混种时以第一要素为准；异种跳过或拆分——M0 允许只写与主导 kind 相同的要素）。
5. 可选字段：把 `Feature::fields` 建成 `OGRFieldDefn`（OFTString），`SetField`。
6. 几何：map 空间顶点 → `OGRPoint` / `OGRLineString` / `OGRPolygon`（注意 `MapScene` 内部 Y 翻转约定与 `ingest` 对称；**写出前用与 ingest 相反的 unflip**，保证 reopen 后 `has_china_extent` / 坐标可读）。
7. `GDALClose`；成功 true。

- [ ] **Step 4: 再跑** `out\map_scene_test.exe` — Expected: PASS（含往返用例）。

- [ ] **Step 5: 不 commit。**

---

### Task 2: 壳接线 — Save 菜单 + Draw Line + `feature_info()` 访问器

**Files:**
- Modify: `src/app/views/browser_view.h`、`src/app/views/browser_view.cc`
- Modify: `src/app/views/README.md`（操作一句）
- Consumes: `MapScene::write_path`
- Produces: 用户可菜单 Draw line / Save；自测可读 FeatureInfo

- [ ] **Step 1: 暴露访问器**

```cpp
// browser_view.h — public, near map_viewport():
  ui::views::FeatureInfo* feature_info() const { return feature_info_; }
  ui::views::AttributeTable* attribute_table() const { return attribute_table_; }
```

（若 `attribute_table_` 成员名不同，以头文件为准。）

- [ ] **Step 2: 菜单**

在 `build_contents` MenuBar（现有 Open / Draw 旁）：

```cpp
  menu->add_item("DrawLine", [this]() {
    run_tool_command("edit.append.linestring");
  });
  menu->add_item("Save", [this]() { on_save_document(); });
```

实现 `on_save_document()`（private）：

```cpp
void BrowserView::on_save_document() {
  auto picked = ui::views::pick_save_file(
      hwnd(), L"GeoJSON\0*.geojson\0All\0*.*\0");
  if (!picked.ok || picked.path.empty()) {
    status("Save cancelled");
    return;
  }
  if (!document_.write_path(picked.path)) {
    status("Save failed");
    return;
  }
  status(std::string("Saved (OGR): ") + picked.path);
}
```

把现有 `catalog.map.save` / `save_as` 分支改为优先 `document_.write_path`（保留状态栏文案）；不要再只走空 `CatalogCall` JSON。

- [ ] **Step 3: 编译 Views**

```bat
build.bat views
```

Expected: `out\SmartGisViews.exe` 链接成功。

- [ ] **Step 4: README** — 在 Open 段落后加：菜单 **DrawLine** = `edit.append.linestring`；**Save** = active 层 GeoJSON 写出。

- [ ] **Step 5: 不 commit。**

---

### Task 3: `--self-test` 钉死 M0 口令

**Files:**
- Modify: `src/app/views/main.cc`
- Modify: `docs/build/ui-testing.md`
- Consumes: Task 1–2 APIs
- Produces: marks `m0-line-ok` / `m0-featureinfo-ok` / `m0-save-ok`；失败码 **60–63**（避开现有 1–54）

在现有 `edit.append.point` 段 **之后**（`selection-ok` 前后均可，建议在 `pan-ok` 之前、china 打开之后，或紧接 `edit-point`）插入独立块：

- [ ] **Step 1: 追加折线**

```cpp
    const size_t before = browser.document()->feature_count();
    if (!browser.run_tool_command("edit.append.linestring")) {
      self_test_detach_maps(browser);
      return 60;
    }
    {
      content::ViewHost* host = browser.edit_view_host();
      tool::Interaction* cur = host->workspace()->stack().current();
      if (!cur || std::strcmp(cur->id(), "draw.linestring") != 0) {
        self_test_detach_maps(browser);
        return 60;
      }
      content::InputEvent d0{};
      d0.kind = content::InputEvent::Kind::kLDown;
      d0.x_px = 20;
      d0.y_px = 20;
      content::InputEvent m1{};
      m1.kind = content::InputEvent::Kind::kMouseMove;
      m1.x_px = 80;
      m1.y_px = 60;
      content::InputEvent d1{};
      d1.kind = content::InputEvent::Kind::kLDown;  // or LUp per StrokeMode::kLine
      d1.x_px = 80;
      d1.y_px = 60;
      // Match draw.linestring completion: typically LDown, Move, LDown/LUp, RDown finish.
      // Copy the exact sequence from gestures_test.cc line-draw case.
      if (!host->dispatch_input(d0) || !host->dispatch_input(m1) ||
          !host->dispatch_input(d1)) {
        self_test_detach_maps(browser);
        return 60;
      }
      // If line needs right-click finish:
      content::InputEvent fin{};
      fin.kind = content::InputEvent::Kind::kRDown;
      fin.x_px = 80;
      fin.y_px = 60;
      (void)host->dispatch_input(fin);
    }
    if (browser.document()->feature_count() <= before) {
      self_test_detach_maps(browser);
      return 60;
    }
    self_test_mark("m0-line-ok");
```

> 输入序列必须以 `src/tool/gestures_test.cc` 中 `edit.append.linestring` 用例为准抄写，不要 invent。

- [ ] **Step 2: FeatureInfo**

```cpp
    {
      app::MapScene* doc = browser.document();
      auto* pane = browser.map_viewport();
      RECT rc{};
      GetClientRect(pane->native_view(), &rc);
      const int vw = rc.right - rc.left;
      const int vh = rc.bottom - rc.top;
      const app::MapScene::Feature* hit =
          doc->hit_test(vw / 2, vh / 2, vw, vh);
      if (!hit) {
        // Fall back: select first feature id from active layer.
        doc->clear_selection();
        // Prefer select_feature(first id) if hit_test empty in headless size.
      } else {
        doc->select_feature(hit->id);
      }
      // Drive the same path BrowserView uses: publish SelectionChanged or
      // call the inspector update helper if extracted.
      ui::views::FeatureInfo* info = browser.feature_info();
      if (!info || info->feature_id().empty()) {
        // After select, BrowserView subscription should fill id; if only
        // MapScene selected, explicitly mirror fields like the EventBus handler.
        self_test_detach_maps(browser);
        return 61;
      }
      self_test_mark("m0-featureinfo-ok");
    }
```

若订阅不会在自测里自动刷，抽出 `BrowserView::refresh_inspector_from_selection()`（private，供 EventBus 与自测调用），**禁止**自测直接摸 `SmtFeature*`。

- [ ] **Step 3: Save 往返**

```cpp
    {
      char tmp[MAX_PATH] = {};
      GetTempPathA(MAX_PATH, tmp);
      std::string out = std::string(tmp) + "smartgis_m0_selftest.geojson";
      DeleteFileA(out.c_str());
      if (!browser.document()->write_path(out)) {
        self_test_detach_maps(browser);
        return 62;
      }
      app::MapScene probe;
      if (!probe.open_path(out) || probe.feature_count() < 1) {
        DeleteFileA(out.c_str());
        self_test_detach_maps(browser);
        return 63;
      }
      DeleteFileA(out.c_str());
      self_test_mark("m0-save-ok");
    }
```

- [ ] **Step 4: 更新 `ui-testing.md`** — 表增加退出码 60–63 与 marks `m0-line-ok` / `m0-featureinfo-ok` / `m0-save-ok`。

- [ ] **Step 5: 跑自测**

```bat
build.bat views
out\SmartGisViews.exe --self-test
```

Expected: exit 0；stderr/旁路 mark 含 `m0-line-ok`、`m0-featureinfo-ok`、`m0-save-ok`（若 mark 写文件，与现有 `self_test_mark` 一致）。

- [ ] **Step 6: 不 commit。**

---

### Task 4: e2e + 差距矩阵收口

**Files:**
- Modify: `docs/build/industry-gap-matrix.md`
- Verify only: `testing/e2e/exe_smoke.cc`（不改除非 Views 超时）
- Modify: 本 plan 勾选

- [ ] **Step 1: 跑门禁**

```bat
build.bat views
out\map_scene_test.exe
out\SmartGisViews.exe --self-test
build.bat e2e
```

Expected: 全部绿；`exe_smoke` 中 `SmartGisViews.exe` PASS。

- [ ] **Step 2: 更新差距矩阵**

在 §1.3 M0 行「主规格」列加上本 plan 链接；§2「桌面壳」SmartGIS 现状改为 **B（M0 自测口令绿）**（未宣称 A，因深度编辑/拓扑仍弱）；§6 M0 验收旁注「见 `2026-09-20-m0-views-main-path.md`」。

- [ ] **Step 3: 勾选本 plan 全部 Task；Status 保持 active 直到用户宣布 M0 landed。**

- [ ] **Step 4: 不 commit**（除非用户要求提交）。

---

## Self-review

| 要求（gap matrix §6 M0） | Task |
| --- | --- |
| 仅 Views 开 GPKG | 已有 `open_path` + self-test china；本 plan 不削弱 |
| 平移缩放 | 已有 `pan-ok` / `wheel-cursor-ok` |
| FeatureInfo | Task 2–3 |
| 追加一条线并保存 | Task 1–3（linestring + `write_path`） |
| `build.bat e2e` 绿 | Task 4 |
| 日常停编 MFC | SP5 前置；本 plan 不拉 `legacy_app` |

占位符扫描：无 TBD；输入序列要求抄 `gestures_test`；`Draft` 字段名实现时核对真源。

---

## Done when

1. [x] `map_scene_test` 含 write 往返且 PASS。  
2. [x] `SmartGisViews.exe --self-test` exit 0，含 `m0-line-ok` / `m0-featureinfo-ok` / `m0-save-ok`。  
3. [x] `build.bat e2e` 绿。  
4. [x] 差距矩阵已回写本 plan 链接与 M0 成熟度。  
5. [x] 无新的终局→legacy include。

**Landed (implementation):** 2026-09-20 — Tasks 1–4 on `master`（无 commit，待用户要求再提交）。
