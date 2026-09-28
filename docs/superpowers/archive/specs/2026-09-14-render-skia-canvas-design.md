<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md) — §Skia canvas chrome (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


Status: superseded (2026-09-28 merge B)
Updated: 2026-09-28

# `ui::gfx` 壳画布：边界与阶段

**Date:** 2026-09-14  
**Scope:** Views 壳 2D paint 的 canvas API 与后端策略（GDI stub + 可选真 Skia；进程启动时运行时切换）。不覆盖地图 2D/3D、RHI、leftover 设备。2026-09-28：职责子目录 + geometry 归属 + scope C；同日锁定运行时双后端（见 § 运行时后端切换）。

## 依据

- 对话已锁定决策（本文件只落边界，不再选型）
- [`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)
- [`docs/superpowers/specs/2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md) §6.3
- 现状代码：`src/ui/gfx/canvas/canvas.h`（GDI/Skia 双后端；见 § 职责子目录）

## 目标

给 `ui::views` 的 `paint_self` 一条**稳定、可测**的 2D 画布 API：短期用 **GDI stub** 补齐缺口；终局在**本机 pin 真 Skia** 且满足准入条件后切换实现，**公开 API 形状不变**。

## 非目标（明确不做）

| 禁止项 | 理由 |
| --- | --- |
| 用 Skia / 本 canvas 画 GIS 矢量、地形、模型 | 地图走 `render::rhi` / leftover；§6.3 |
| 把 Skia 当 widget kit | Widgets 只在 `ui::views` |
| 用当前 `render::rhi` 直接画壳 chrome | RHI 是地图 GPU Facade，不是壳 paint |
| 引入 **GDI+** | 已锁定；短期只用 GDI32 |
| Drive-by vendor 整树 Chromium / Skia 进仓 | 真 Skia 仅本机 pin + 可选 GN；默认不进 `src_all` |
| Qt / WinUI 当终局壳 / WebView2 回潮 | 仓库规则 |

## 分层

```
CLI --shell-canvas= / env SMT_SHELL_CANVAS
        │
        ▼
ui::gfx::apply_shell_canvas_preference  （进程启动一次）
        │
ui::views::*::paint_self(Canvas*)
        │
        ▼
ui::gfx::Canvas     ← 公开 API（本设计锁定表面）
        │
   ┌────┴────┐
   │         │
GDI stub   真 Skia（同二进制可选链入）
(gdi32)    (smt_has_skia + 本机 pin；运行时可选)
```

- Include：`"ui/gfx/<area>/...."`；命名空间 `ui::gfx`（两层）。
- GN：`//src/ui/gfx:gfx` 经 `//:ui_views`；**不**进 `//src:src_all` / `render_all`。
- 地图 HWND 仍挂 `MapViewport`；壳 paint 与地图 present **进程内可并存、职责不混**。

## 现状缺口（对照 `paint_self`）

今日 API：

```cpp
void fill_rect(int x, int y, int w, int h, Color color);
void draw_text(int x, int y, const wchar_t* text, Color color);
```

Views 侧已有的 workaround / 缺口：

| 需求 | 今日做法 / 缺口 |
| --- | --- |
| 边框 / focus ring | `theme.cc` 用 4 次 1px `fill_rect` |
| 轴线 / 细线 | `chart_view` 用 2px `fill_rect` 冒充 |
| 裁剪滚动内容 | **无** `clip`；ScrollView / Table 依赖布局不画越界 |
| 按文字量 preferred size | **无** `measure_text`；控件写死宽高 |
| 描边矩形 | **无** `stroke_rect` |
| 折线 / 分隔线 | **无** `draw_line` |
| 嵌套裁剪栈 | **无** `save` / `restore` |

## 最小公开 API（锁定）

`Color` 保持 ARGB `uint32_t`（`color.h`）。整数像素；无浮点几何（YAGNI）。

```cpp
namespace ui::gfx {

class Canvas {
 public:
  Canvas(HDC hdc, int width, int height);  // v1 / GDI stub 构造；真 Skia 阶段可另增工厂，勿破坏现有签名

  void fill_rect(int x, int y, int w, int h, Color color);
  void stroke_rect(int x, int y, int w, int h, Color color, int stroke_width = 1);
  void draw_line(int x0, int y0, int x1, int y1, Color color, int stroke_width = 1);
  void draw_text(int x, int y, const wchar_t* text, Color color);

  // Returns ink size in pixels; empty / null text → {0,0}.
  Size measure_text(const wchar_t* text) const;

  void clip_rect(int x, int y, int w, int h);  // intersect with current clip
  void save();
  void restore();

  int width() const;
  int height() const;
  HDC hdc() const;  // GDI stub only; 真 Skia 实现可返回 nullptr，调用方不得依赖 HDC 画 GIS
};

struct Size {
  int width = 0;
  int height = 0;
};

}  // namespace ui::gfx
```

说明：

- `Size` 唯一归属 `ui/gfx/geometry/size.h`；`Canvas::measure_text` 与布局共用。Views 经 `using` 再导出，不维护第二份定义。
- `stroke_width < 1` 视为 1。`w`/`h` ≤ 0 的 fill/stroke/clip 为 no-op。
- **不做**（仍锁定）：路径/渐变/变换矩阵（Transform）、range、gpu-in-gfx、整树 Chromium。薄 `Image` / `Font` / `Animation` 见下方 scope C §。

## 阶段

| 阶段 | 内容 | 完成判据 |
| --- | --- | --- |
| **A** GDI stub API 补齐 | 实现上表方法；单测覆盖 | `views_unittests` + 新增 canvas 用例绿 |
| **B** Views 消费缺口 API | focus ring / chart 轴改用 stroke/line；可选 `measure_text` 收紧 Label/Button 宽 | 壳视觉不回归；`--self-test` 绿 |
| **C** 真 Skia 准入准备 | GN 可选开关 + 本机 pin 文档；默认仍 GDI | 无 pin 时行为与 A 相同；有 pin 时可编真后端 |
| **D** 真 Skia TU | `canvas_skia.cc` + pin；与 GDI **同链**（非互斥） | `smt_has_skia=true` 可链接 |
| **E** 运行时切换 | 进程级 backend 偏好；CLI/env；无能力回落 GDI | 见 § 运行时后端切换 |

## § 运行时后端切换（2026-09-28）

**Decision:** 用户选定 — 同一 `SmartGisViews` / `views_unittests` 二进制内 **运行时** 选 GDI 或 Skia；无本机 pin 时仍必须能编（仅 GDI）；CLI 优先于环境变量；**默认 gdi**。

| 项 | 约定 |
| --- | --- |
| 偏好 API | `ui/gfx/canvas/shell_canvas_backend.h`：`ShellCanvasBackend`、`apply_shell_canvas_preference`、`is_skia_backend_available` |
| CLI | `--shell-canvas=gdi\|skia`（`ViewsLaunchOptions.shell_canvas`） |
| Env | `SMT_SHELL_CANVAS=gdi\|skia`（CLI 未设时） |
| 默认 | `gdi` |
| 回落 | 请求 `skia` 但未链入 / 创建失败 → `gdi` + stderr 一行日志 |
| 时机 | **进程启动一次**；不做 paint 中途热切 |
| 录制 Canvas | `hdc == nullptr`（DisplayList 录制）始终走轻量 GDI backend |
| Views | `paint_self` **无** `#ifdef SMT_HAS_SKIA` |
| GN | `smt_has_skia=true` + pin → 同时编 `canvas_gdi.cc` + `canvas_skia.cc`；否则 `canvas_skia_stub.cc`（`create_skia_*` → nullptr） |
| 实现形状 | 多态 `Canvas::Backend`；`canvas.cc` 派发；工厂在 gdi/skia/stub TU |

### 非目标（本 §）

- 不默认把产品切到 Skia（默认仍 gdi）
- 不热切换、不 UI 设置页
- 不 vendor 整树 Skia；不进 `src_all`

## 真 Skia 准入条件（链入二进制；默认偏好仍为 gdi）

1. **本机 pin**：类似 FlyCube，仓库内仅 junction / `args` 路径指向本机 Skia 检出；**禁止**把整树 Skia 提交进 `third_party/` 当 drive-by vendor。
2. **GN 显式开启**（建议 `smt_has_skia=true`）；默认 `false`，CI / 日常 `build.bat` 不依赖 Skia 源树。
3. **不进 `src_all`**：`//src/ui/gfx:gfx` 仍只经 `//:ui_views`；真 Skia 目标不得被 `group("all")` 默认拉起。
4. **公开 API 不变**：Views 只 `#include "ui/gfx/canvas/canvas.h"`；无 `#ifdef` 泄漏到 `paint_self`。
5. **测试**：GDI stub 与真 Skia（若本机开启）均能跑通同一套 canvas 行为测试（像素容差可放宽到「非空 / 尺寸正确」级，不做位图黄金图）。
6. **许可证 / 构建**：Skia 构建脚本与依赖清单写入 `src/ui/gfx/README.md`；失败时 fallback GDI，不硬 fail 整仓。

未满足任一条 → **保持 GDI stub 为默认实现**。

## 测试策略

| 入口 | 用途 |
| --- | --- |
| `out/views_unittests.exe`（可 `--self-test`） | 现有 Views 回归 + 新增 canvas API 用例 |
| `out/SmartGisViews.exe --self-test` | 产品壳冒烟（不替代单元断言） |
| （可选）`//src/ui/gfx:gfx_unittests` | 若 canvas 用例过胖，可从 `views_unittests` 拆出；仍经 `ui_views` 编，不进 `src_all` |

## 关系文档

- 实现计划：[`../plans/2026-09-14-render-skia-canvas.md`](../plans/2026-09-14-render-skia-canvas.md)（历史 canvas API；子目录迁移步骤见下 §）
- As-built 终局：[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)
- 控件 / 迁移规格不改边界，仅消费本 canvas API
- Toolkit 子目录范式：[`2026-09-19-ui-views-subdir-responsibility-design.md`](2026-09-19-ui-views-subdir-responsibility-design.md)
- Compositor 消费 DisplayList / ShellRaster：[`2026-09-13-ui-views-controls-design.md`](2026-09-13-ui-views-controls-design.md) § UI compositor thread

---

## § 职责子目录 + geometry 归属（scope C，2026-09-28）

**Decision:** 用户批准范围 **C** — 方案 2 职责分区 + P1 薄 image/font + animation 骨架。

### 布局（锁定）

```
src/ui/gfx/
  BUILD.gn  skia.gni  README.md  skia.h / skia.cc
  geometry/     point.h size.h rect.h insets.h
  color/        color.h
  canvas/       canvas.h canvas.cc | canvas_skia.cc
  display_list/ display_list.h display_list.cc
  raster/       shell_raster.h paint_stats.h/.cc
  image/        image.h image.cc          # 薄 BGRA8
  font/         font.h font.cc            # Font + measure_text_with_font
  animation/    animation.h animation.cc  # linear skeleton
```

| 约定 | 值 |
| --- | --- |
| Include | `"ui/gfx/<area>/...."`；**无**根转发 shim |
| NS | 两层 `ui::gfx` + `detail`；目录不升第三语义层 |
| GN | 单 `//src/ui/gfx:gfx` |
| 几何 | `Point` / `Size` / `Rect` / `Insets` 属 `ui::gfx`；`ui::views` `using` 再导出 |

### Scope C 骨架边界

| 模块 | 做 | 不做 |
| --- | --- | --- |
| `image` | BGRA8 缓冲、`clear`、尺寸 | ImageSkia、解码管道、GIS 纹理 |
| `font` | `Font` 描述 + `measure_text_with_font` | FontList / RenderText / shaping |
| `animation` | `start`/`stop`/`step(0..1)` 虚回调 | AnimationContainer、多曲线、自有计时线程 |
| 全体 | — | Transform / range / gpu-in-gfx / vendor Chromium |

### 迁移步骤（已落地）

1. [x] 新增 `geometry/`；从 `view.h` 抽出 Point/Size/Rect；补 `Insets`
2. [x] 现有 canvas/color/display_list/shell_raster/paint_stats 迁入职责目录；改 `BUILD.gn`
3. [x] 全仓 `#include "ui/gfx/..."` 对齐新路径（views / gpu / tests）
4. [x] 薄 image / font / animation 编入 `:gfx`
5. [x] 更新本 living、`src/ui/gfx/README.md`、as-built、compositor 锚点

验证（人类执行）：`build.bat views` → `out\views_unittests.exe --self-test`
