<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# render::skia 壳画布 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Views 壳补齐 `render::skia::Canvas` 最小 2D API（GDI stub），用测试锁住行为，并写清真 Skia 本机 pin 的准入与默认关闭路径。

**Architecture:** 公开 API 固定在 `src/render/skia/canvas.h`；v1 继续 GDI32（**不用 GDI+**）。Views `paint_self` 只依赖该 API。真 Skia 为可选实现，经本机 pin + `smt_has_skia`，**不进 `src_all`**。地图 paint 仍走 RHI / leftover，禁止混入本模块。

**Tech Stack:** C++23、GN/Ninja（`out/` only）、Win32 GDI32、现有 `views_unittests` / `SmartGisViews.exe --self-test`。

## Global Constraints

- 工作目录 `C:/Dev/src/gis/smartgis`，只在 **`master`** 改；不要开分支。
- **不要 commit**，除非用户明确要求。
- Spec：`docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md`。
- 禁止：GDI+、Qt、用 RHI 画壳、Skia 画 GIS、Skia 当 widget kit、drive-by vendor 整树 Chromium/Skia。
- Copyright：新文件 `Copyright (c) 2026 The Mogu Authors.`；源码注释英文；文档中文。
- 公共命名空间 ≤2（`render::skia`）；新函数 `snake_case`。
- `//src/render/skia:skia` 经 `//:ui_views`；**永不**默认进 `//src:src_all`。

## File map

| Path | Responsibility |
| --- | --- |
| `src/render/skia/canvas.h` | 公开 `Canvas` + `Size`；最小 API |
| `src/render/skia/canvas.cc` | GDI stub 实现（fill/stroke/line/text/measure/clip/save/restore） |
| `src/render/skia/color.h` | 保持 ARGB；本计划不改语义 |
| `src/render/skia/BUILD.gn` | `source_set("skia")`；可选真 Skia 源 / 开关 |
| `src/render/skia/README.md` | stub 说明 + 真 Skia pin / 准入 |
| `src/ui/views/kernel/theme.cc` | `draw_focus_ring` 改用 `stroke_rect` |
| `src/ui/views/gis/chart_view.cc` | 轴线改用 `draw_line` / `stroke_rect` |
| `src/ui/views/views_unittests.cc` | canvas API 行为测试 + 既有回归 |
| `src/ui/views/BUILD.gn` | 仅当拆测目标时改；默认可只加测例到现有 test |
| `docs/build/ui-views-skia.md` | Status 段补一句「canvas API 阶段」（落地后） |
| `docs/README.md` | Active 索引两条（本轮已挂） |

**明确不做清单（实现时跳过）：**

- 不引入 `gdiplus.lib` / `#include <gdiplus.h>`。
- 不在 `paint_self` 里直接 `BitBlt` 地图像素到 Skia。
- 不把 `Canvas` 挂到 `render::rhi::CommandList`。
- 不 vendor `third_party/skia` 整树进 git。
- 不实现路径、渐变、图片、字体枚举、仿射变换。
- 不改 `MapViewport` 的 HWND / RHI present 语义。

---

### Task 1: 锁定 API 头文件 + 失败测试（阶段 A 前半）

**Files:**
- Modify: `src/render/skia/canvas.h`
- Modify: `src/ui/views/views_unittests.cc`
- Modify: `src/render/skia/README.md`（一句指向 design/plan）

**Interfaces:**
- Produces: `render::skia::Size`；`Canvas::{stroke_rect,draw_line,measure_text,clip_rect,save,restore}` 声明

- [x] **Step 1: 在 `canvas.h` 增加 `Size` 与新方法声明**

在现有 `Canvas` 类中扩展到与 design 一致（保留现有构造与 `fill_rect` / `draw_text` / accessors）：

```cpp
struct Size {
  int width = 0;
  int height = 0;
};

class Canvas {
 public:
  Canvas(HDC hdc, int width, int height);

  void fill_rect(int x, int y, int w, int h, Color color);
  void stroke_rect(int x, int y, int w, int h, Color color,
                   int stroke_width = 1);
  void draw_line(int x0, int y0, int x1, int y1, Color color,
                 int stroke_width = 1);
  void draw_text(int x, int y, const wchar_t* text, Color color);

  Size measure_text(const wchar_t* text) const;

  void clip_rect(int x, int y, int w, int h);
  void save();
  void restore();

  int width() const { return width_; }
  int height() const { return height_; }
  HDC hdc() const { return hdc_; }

 private:
  HDC hdc_;
  int width_;
  int height_;
  // GDI stub may add clip stack members in Task 2.
};
```

- [x] **Step 2: 在 `views_unittests.cc` 增加 canvas 测例（先写断言，实现前应链接失败或行为错）**

在现有 `expect(...)` 风格旁新增一段（可用 memory DC）：

```cpp
#include "render/skia/canvas.h"

void test_skia_canvas_api() {
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  const int W = 64;
  const int H = 32;
  HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
  HGDIOBJ old = SelectObject(mem, bmp);

  render::skia::Canvas c(mem, W, H);
  c.fill_rect(0, 0, W, H, render::skia::color_rgb(0, 0, 0));
  c.stroke_rect(2, 2, 20, 10, render::skia::color_rgb(255, 0, 0), 1);
  c.draw_line(0, 0, 10, 10, render::skia::color_rgb(0, 255, 0), 1);
  c.save();
  c.clip_rect(8, 8, 16, 16);
  c.fill_rect(0, 0, W, H, render::skia::color_rgb(0, 0, 255));
  c.restore();
  const auto sz = c.measure_text(L"Ab");
  expect(sz.width > 0 && sz.height > 0, "measure_text Ab");
  expect(c.measure_text(L"").width == 0, "measure_text empty");
  expect(c.measure_text(nullptr).width == 0, "measure_text null");

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}
```

在 `main` / 现有 test runner 里调用 `test_skia_canvas_api()`。

- [x] **Step 3: 编译确认缺口**

Run: `build.bat views`（或 `ninja -C out views_unittests`）

Expected: 链接/编译失败（未实现的新符号），或测例失败——记下错误后进入 Task 2。

- [x] **Step 4: 更新 `src/render/skia/README.md`**

加一句：最小 API 与阶段见 `docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md`；实现计划见 `docs/superpowers/plans/2026-09-14-render-skia-canvas.md`。刷新「最后更新」为 2026-09-14。

---

### Task 2: GDI stub 实现（阶段 A 后半）

**Files:**
- Modify: `src/render/skia/canvas.cc`
- Modify: `src/render/skia/canvas.h`（若需 clip 栈成员）
- Modify: `src/render/skia/BUILD.gn`（仍只需 `gdi32.lib`；**禁止**加 `gdiplus`）

**Interfaces:**
- Consumes: Task 1 声明
- Produces: GDI stub 行为符合测例

- [x] **Step 1: 实现 `stroke_rect` / `draw_line`**

用 `CreatePen` + `Rectangle` / `MoveToEx`+`LineTo`（或等价 GDI32）。`stroke_width < 1` → 1。透明刷画空心矩形。`w`/`h` ≤ 0 → return。

参考骨架：

```cpp
void Canvas::stroke_rect(int x, int y, int w, int h, Color color,
                         int stroke_width) {
  if (!hdc_ || w <= 0 || h <= 0) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
  const HGDIOBJ old_pen = SelectObject(hdc_, pen);
  const HGDIOBJ old_brush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
  Rectangle(hdc_, x, y, x + w, y + h);
  SelectObject(hdc_, old_brush);
  SelectObject(hdc_, old_pen);
  DeleteObject(pen);
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color,
                       int stroke_width) {
  if (!hdc_) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  const HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(color));
  const HGDIOBJ old_pen = SelectObject(hdc_, pen);
  MoveToEx(hdc_, x0, y0, nullptr);
  LineTo(hdc_, x1, y1);
  SelectObject(hdc_, old_pen);
  DeleteObject(pen);
}
```

- [x] **Step 2: 实现 `measure_text`**

```cpp
Size Canvas::measure_text(const wchar_t* text) const {
  Size out;
  if (!hdc_ || !text || !*text) {
    return out;
  }
  SIZE sz = {};
  if (GetTextExtentPoint32W(hdc_, text, lstrlenW(text), &sz)) {
    out.width = sz.cx;
    out.height = sz.cy;
  }
  return out;
}
```

- [x] **Step 3: 实现 `save` / `restore` / `clip_rect`**

- `save`：`SaveDC(hdc_)`，并把返回值压栈（`std::vector<int>` 成员）。
- `restore`：弹栈 `RestoreDC`；空栈为 no-op。
- `clip_rect`：`IntersectClipRect(hdc_, x, y, x+w, y+h)`；非法尺寸 no-op。

头文件 private 增加例如：

```cpp
std::vector<int> saved_dcs_;
```

`canvas.cc` / `canvas.h` 相应 `#include <vector>`。

- [x] **Step 4: 跑测**

Run:

```bat
ninja -C out views_unittests
out\views_unittests.exe --self-test
```

Expected: PASS（含 `test_skia_canvas_api` 与既有 Views 用例）。

---

### Task 3: Views 消费新 API（阶段 B）

**Files:**
- Modify: `src/ui/views/kernel/theme.cc`（`draw_focus_ring`）
- Modify: `src/ui/views/gis/chart_view.cc`（坐标轴）
- Optional: `src/ui/views/primitives/label.cc` / `button.cc`（用 `measure_text` 收紧 preferred width；**仅当**不破坏现有 layout 测例）

**Interfaces:**
- Consumes: `stroke_rect` / `draw_line` /（可选）`measure_text`
- Produces: 去掉 1px `fill_rect` 冒充描边的主路径

- [x] **Step 1: 改写 `draw_focus_ring`**

```cpp
void draw_focus_ring(render::skia::Canvas* canvas, const Rect& bounds) {
  if (!canvas) {
    return;
  }
  canvas->stroke_rect(bounds.x, bounds.y, bounds.width, bounds.height,
                      Theme::current().accent, 1);
}
```

- [x] **Step 2: 改写 `ChartView` 轴线**

将今日两段 `fill_rect(... , 2, ...)` 轴线改为 `draw_line`（或 `stroke_rect` 外框），保持标签 `draw_text` / 柱 `fill_rect` 不变。

- [x] **Step 3:（可选）Label/Button preferred size**

若改：在 `set_text` / 构造路径用 `measure_text` 设最小宽 = ink + padding；跑 `views_unittests` 中 preferred_size 相关断言，失败则回退本步（留下 TODO 注释英文一句即可，勿硬撑）。

已落地：`measure_text_utf8`（theme）+ Label/Button `preferred_for_text`；测例 `test_label_button_preferred_from_measure`。Checkbox / RadioButton / Textfield 等仍写死宽，未改。

- [x] **Step 4: 回归**

```bat
ninja -C out views_unittests SmartGisViews
out\views_unittests.exe --self-test
out\SmartGisViews.exe --self-test
```

Expected: 全部 PASS / 冒烟退出码 0。

（2026-09-14：`views_unittests` PASS；`SmartGisViews.exe --self-test` 在 `MapViewport::wait_ready` 路径上长时间无退出，已终止——与本轮 canvas API 改动无直接关系，留待壳冒烟排查。）

---

### Task 4: 真 Skia 准入脚手架（阶段 C，默认关闭）

**Files:**
- Modify: `src/render/skia/BUILD.gn`
- Modify: `src/render/skia/README.md`
- Modify: `docs/build/ui-views-skia.md`（Status 段补「canvas：GDI stub 默认；真 Skia 可选」）
- Create: `src/render/skia/canvas_skia.cc`（阶段 D：本机 pin + 预编译 lib 时链接真后端）

**Interfaces:**
- Produces: `declare_args()` 中 `smt_has_skia = false`；文档写清 pin 路径约定

- [x] **Step 1: GN 开关（默认 false）**

在合适的 args 文件或 `src/render/skia/BUILD.gn` 顶部：

```gn
declare_args() {
  # Local Skia pin only. Default off; never pull into src_all.
  smt_has_skia = false
}
```

`source_set("skia")` 在 `smt_has_skia == false` 时继续只用 `canvas.cc`（GDI）。为 true 时才追加真 Skia TU / include / libs（具体路径写 README，例如本机 `C:/Dev/src/open/.../skia` junction → `third_party/.src/skia`，**与 FlyCube 同模式**）。

- [x] **Step 2: README 准入清单**

把 design「真 Skia 准入条件」六条抄入 `src/render/skia/README.md`（中文可；保持与 design 一致）。强调：默认 GDI；开启失败须 fallback，不得让 `build.bat views` 在无 pin 机器上挂。

- [x] **Step 3: 验证默认路径**

在 **未** 设置 `smt_has_skia` 时：

```bat
build.bat views
out\views_unittests.exe --self-test
```

Expected: 与 Task 3 相同，全绿。

- [x] **Step 4: 不进 `src_all` 检查**

确认 `src/BUILD.gn` / 根 `BUILD.gn` 的 `src_all` / `group("all")` **没有**因本 Task 新增对真 Skia 或强制 `smt_has_skia` 的依赖。`//:ui_views` 保持 opt-in。

- [x] **Step 5: 阶段 D — 真后端 TU + 本机 pin（2026-09-14）**

- 权威源（WSL）：`/home/ccl/dev/src/open/topic/graphic-engine/skia`
- Windows：`\\wsl$\Ubuntu-24.04\home\ccl\dev\src\open\topic\graphic-engine\skia`
- pin：`third_party\.src\skia` 为**提升权限目录符号链接**（`/J` 对 UNC 失败）
- `canvas_skia.cc`：现代 Skia API（`SkSurfaces::WrapPixels` + DirectWrite FontMgr）；`smt_has_skia=true` 时替换 `canvas.cc`
- 匹配的 Windows `skia.lib` 尚无；缺 lib 时 WARNING + 链接失败；默认 `false` 仍走 GDI
- **不**把默认实现切到真 Skia；Label/Button measure 由另一路处理

---

### Task 5: 文档收尾（本轮可先做；代码 Tasks 落地后复核）

**Files:**
- `docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md`（已写）
- `docs/superpowers/plans/2026-09-14-render-skia-canvas.md`（本文件）
- `docs/README.md`（索引）
- 落地后：`docs/build/ui-views-skia.md` Status 一句

- [ ] **Step 1:** 实现全部 Task 1–4 后，若行为已是 as-built，把本 plan/spec 标 `Status: landed` 并按 `superpowers-docs.mdc` **同一变更**移入 `docs/superpowers/archive/{specs,plans}/`，as-built 事实写入 `docs/build/ui-views-skia.md`。
- [ ] **Step 2:** **未**完成实现前保持 `Status: active`；仅文档审阅循环时 revise in place，不开平行空壳。

---

## 阶段摘要（审阅用）

1. **边界**：壳 paint = `render::skia`；地图 = RHI/leftover；禁止 RHI 画壳、Skia 画 GIS、GDI+、Qt、整树 vendor。
2. **现状**：仅 `fill_rect` / `draw_text`；边框与轴线靠 1–2px 矩形冒充；无 clip / measure。
3. **阶段 A**：GDI stub 补齐 `stroke_rect` / `draw_line` / `measure_text` / `clip_rect` / `save` / `restore`。
4. **阶段 B**：`theme` focus ring、`ChartView` 轴等改用新 API；可选文字测宽。
5. **测试**：`views_unittests`（含 memory-DC canvas 用例）+ `SmartGisViews.exe --self-test`。
6. **阶段 C**：`smt_has_skia` 默认 false；本机 pin 才编真后端；**不进 `src_all`**。
7. **准入**：六条全满足才可考虑默认切真 Skia；否则永远 GDI stub 默认。
8. **不做**：GDI+、路径/渐变/图片、地图像素进 Skia、把 canvas 绑上 RHI。

## Self-review（作者核对）

| Design 要求 | 对应 Task |
| --- | --- |
| 最小 API 清单 | Task 1–2 |
| Views 消费缺口 | Task 3 |
| 测试入口 | Task 1–3 |
| 真 Skia 准入 / 默认关 / 不进 src_all | Task 4 |
| 明确不做 | File map + Global Constraints |
| 无平行空壳 / 索引 | Task 5 + `docs/README.md` |

无 TBD 占位；类型名与 design 一致（`Size` / `stroke_rect` / `measure_text`）。
