<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Split leftover render engines into `src/legacy/render`

**Date:** 2026-09-13  
**Status:** accepted  
**Related:** 伞状深度设计 [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md)；RHI [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；布局 [`../../build/src-layout.md`](../../build/src-layout.md)。  
**Scope:** 物理子目录重构（方案 A）：整包平移 + include/GN 全改名，**不留** `src/render/<old>` 转发头。

## Goal

`src/render/` 只保留终局路径（`rhi` / `scene` GpuScene / `skia` / `math` + 极薄 `SmtRender` 桩）。2010 leftover 设备与三维引擎整包迁到 **`src/legacy/render/<module>`**，默认 **不进** `src_all`。调用方 include 与 GN label 同步改为 `legacy/render/…`。

## Non-goals

- 不改 leftover `dll_stem` / `Smt_*` 导出 ABI（仅路径与 GN label）。
- 不复活 D3D9；不加 Qt；不在 leftover 里加 Assimp / FlyCube。
- 不借机重写 leftover 业务逻辑。
- 本轮 **不要求** `smt_build_app` / 依赖 leftover 的 plugin、xview 全绿（允许断；另编 `legacy_render_all`）。
- 不恢复已删除的 `branches/`。

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| 迁出范围 | 除终局外几乎全迁：`gdi` / `gdi_simple` / `gl` / `render3d` / `scene3d` / `model3d` / `terrain` / `pointcloud`；完整 Bridge + `leftover_*` |
| 落点 | `src/legacy/render/<module>`（新层；两层嵌套上限） |
| 改名策略 | **A**：无转发头；`"render/<leftover>/…"` → `"legacy/render/<module>/…"` |
| `src_all` | 只含终局 `//src/render:render_all` |
| 可选编 | `//src/legacy/render:legacy_render_all`（不进 `src_all`） |
| MFC present | 允许暂时断；终局 `//src/render:render` 为最小桩 |
| `leftover_mesh` / `leftover_record` / `leftover_session` | 随 legacy，放 `legacy/render/bridge` |
| 完整 `renderdevice` / `renderer` | 随 bridge 进 legacy；`src/render` 留桩或仅 `rhi`/`scene` 图 |

## Target tree

### `src/render/`（终局）

```
src/render/
  BUILD.gn          # rhi source_set + thin render DLL stub + lean render_all
  rhi/
  scene/            # GpuScene only (no leftover_*)
  skia/
  math/
  README.md         # endgame-focused
```

### `src/legacy/render/`（leftover）

```
src/legacy/render/
  BUILD.gn          # group("legacy_render_all")
  bridge/           # renderdevice, renderer, leftover_session, leftover_mesh, leftover_record
  gdi/
  gdi_simple/
  gl/
  render3d/
  scene3d/
  model3d/
  terrain/
  pointcloud/
```

## Dependency rules

| Consumer | May depend on |
| --- | --- |
| `src_all` / 新代码 | `render::rhi`, `render::scene::GpuScene`, `render/math`, `render/skia` |
| MFC / plugin / xview / tool_group（本轮） | `legacy/render/…`（改断点后仍可编 `legacy_render_all` + `smt_build_app`） |
| `legacy_render` → `render/rhi` / `render/math` | 允许（单向靠 Facade） |
| `render` 终局 → `legacy_render` | **禁止** |

## Success criteria

1. `src/render/` 下不再存在 `gdi` / `gl` / `render3d` / `scene3d` / `model3d` / `terrain` / `pointcloud` / `gdi_simple` 目录。
2. 仓库内无 `#include "render/(gdi|gl|render3d|scene3d|model3d|terrain|pointcloud|gdi_simple)/…"`（应已改为 `legacy_render`）。
3. `//src/render:render_all` 仅终局目标；`build.bat` 日常 `src_all` 不拉 leftover DLL。
4. `docs/build/src-layout.md` 与伞状设计 §6.4 路径表已改为 `legacy_render`。
5. Copyright / snake_case / 两层 namespace 规则不变。

## Doc updates (same change)

- `docs/build/src-layout.md` — path map + render 层说明  
- `docs/superpowers/specs/2026-09-13-model-render-compute-design.md` — §6.4 路径改为 `legacy_render`；注明 present 缝本轮可断  
- `src/render/README.md` — 终局说明 + 指向 `legacy_render`  
- 根 `README.md` — 若模块表仍写 leftover 挂在 `render/`，同步改（否则可不动）
