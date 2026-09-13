<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# `render::skia` 壳画布：边界与阶段

**Date:** 2026-09-14  
**Scope:** Views 壳 2D paint 的 canvas API 与后端策略（短期 GDI stub → 条件准入真 Skia）。不覆盖地图 2D/3D、RHI、leftover 设备。

## 依据

- 对话已锁定决策（本文件只落边界，不再选型）
- [`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)
- [`docs/superpowers/specs/2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md) §6.3
- 现状代码：`src/render/skia/canvas.h`（仅 `fill_rect` / `draw_text`）

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
ui::views::*::paint_self(Canvas*)
        │
        ▼
render::skia::Canvas     ← 公开 API（本设计锁定表面）
        │
   ┌────┴────┐
   │         │
GDI stub   真 Skia（可选，准入后）
(gdi32)    (本机 pin；默认关)
```

- Include：`"render/skia/..."`；命名空间 `render::skia`（两层）。
- GN：`//src/render/skia:skia` 经 `//:ui_views`；**不**进 `//src:src_all` / `render_all`。
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
namespace render::skia {

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

}  // namespace render::skia
```

说明：

- `Size` 可放在 `canvas.h` 或薄 `types.h`；**不要**引入 `ui::views::Size` 依赖（render 不依赖 views）。
- `stroke_width < 1` 视为 1。`w`/`h` ≤ 0 的 fill/stroke/clip 为 no-op。
- **不**暴露路径、渐变、图片、变换矩阵、字体枚举（后续若需要另开修订；本轮 YAGNI）。

## 阶段

| 阶段 | 内容 | 完成判据 |
| --- | --- | --- |
| **A** GDI stub API 补齐 | 实现上表方法；单测覆盖 | `views_unittests` + 新增 canvas 用例绿 |
| **B** Views 消费缺口 API | focus ring / chart 轴改用 stroke/line；可选 `measure_text` 收紧 Label/Button 宽 | 壳视觉不回归；`--self-test` 绿 |
| **C** 真 Skia 准入准备 | GN 可选开关 + 本机 pin 文档；默认仍 GDI | 无 pin 时行为与 A 相同；有 pin 时可编真后端 |
| **D**（可选）切真 Skia | 同一 API 换实现；GDI 保留为 fallback | 见下方准入条件 |

## 真 Skia 准入条件（全部满足才允许默认切）

1. **本机 pin**：类似 FlyCube，仓库内仅 junction / `args` 路径指向本机 Skia 检出；**禁止**把整树 Skia 提交进 `third_party/` 当 drive-by vendor。
2. **GN 显式开启**（建议 `smt_has_skia=true`）；默认 `false`，CI / 日常 `build.bat` 不依赖 Skia 源树。
3. **不进 `src_all`**：`//src/render/skia:skia` 仍只经 `//:ui_views`；真 Skia 目标不得被 `group("all")` 默认拉起。
4. **公开 API 不变**：Views 只 `#include "render/skia/canvas.h"`；无 `#ifdef` 泄漏到 `paint_self`。
5. **测试**：GDI stub 与真 Skia（若本机开启）均能跑通同一套 canvas 行为测试（像素容差可放宽到「非空 / 尺寸正确」级，不做位图黄金图）。
6. **许可证 / 构建**：Skia 构建脚本与依赖清单写入 `src/render/skia/README.md`；失败时 fallback GDI，不硬 fail 整仓。

未满足任一条 → **保持 GDI stub 为默认实现**。

## 测试策略

| 入口 | 用途 |
| --- | --- |
| `out/views_unittests.exe`（可 `--self-test`） | 现有 Views 回归 + 新增 canvas API 用例 |
| `out/SmartGisViews.exe --self-test` | 产品壳冒烟（不替代单元断言） |
| （可选）`//src/render/skia:skia_unittests` | 若 canvas 用例过胖，可从 `views_unittests` 拆出；仍经 `ui_views` 编，不进 `src_all` |

## 关系文档

- 实现计划：[`../plans/2026-09-14-render-skia-canvas.md`](../plans/2026-09-14-render-skia-canvas.md)
- As-built 终局：[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)
- 控件 / 迁移规格不改边界，仅消费本 canvas API
