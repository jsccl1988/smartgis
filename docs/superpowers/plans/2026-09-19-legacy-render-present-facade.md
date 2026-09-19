<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: Legacy Present / Paint Facade（SP2）

**Date:** 2026-09-19  
**Status:** active  
**Spec:** [`../specs/2026-09-19-legacy-render-present-facade-design.md`](../specs/2026-09-19-legacy-render-present-facade-design.md)

## Goal

恢复 `bind_rhi_present` strangler（HWND → `leftover_session`），去掉「在 GDI 共享 HWND 上自动创建 FlyCube」的危险路径；并把主 GDI / gdi_simple `RenderMap` 接到同一 Null 录制 Facade。

## Tasks

### Task 1: Spec（本文件 + design）

- [x] Living design `2026-09-19-legacy-render-present-facade-design.md`
- [x] 本 plan

### Task 2: LeftoverRecorder present bind

- [x] `native_window()` accessor
- [x] `bind_present_hwnd(void*)`：设 HWND + 若无 Device 则 owned `kNull`
- [x] `ensure_device`：删除「有 HWND 则 preferred_gpu」分支；仅 Null
- [x] 注释说明为何禁止同 HWND FlyCube

### Task 3: `bind_rhi_present` strangler

- [x] `renderdevice.cpp`：调用 `leftover_session().bind_present_hwnd`
- [x] 头文件注释与设计一致（薄 RHI wrap，非 FlyCube）

### Task 4: Tests（最小缝）

- [x] 扩展 `leftover_session_test`：`bind_rhi_present` → HWND + Null backend
- [x] 扩展 `leftover_record_test`：有 HWND 仍 Null
- [x] 构建并跑通：`leftover_session_test` / `leftover_record_test` / `leftover_mesh_test`

### Task 5: GDI RenderMap → leftover_session

- [x] 抽出 `leftover_record_map_frame`（Null begin/record_map/finish）
- [x] 主 GDI `SmtGdiRenderDevice::RenderMap` + `SmtGdiRenderThread::RenderMap` 接通
- [x] `gdi_simple` 改用同一 helper
- [x] **顺序锁定**：helper 必须在 GDI 画完**之后**调用（先 tessellate 会导致 map buffer 全白；`gdi_map_paint_test` 对照验证）
- [x] `leftover_session_test` 覆盖 helper（空图 / 非法尺寸 / null map）
- [x] `gdi_map_paint_test` 绿（non-white samples > 20）

### Task 6: GL Init 对称接线

- [x] `SmtGLRenderDevice::Init` 调用 `bind_rhi_present`（SwapBuffers 仍独占本 HWND）
- [x] `gdi/README.md` / `gl/README.md` 注明 Present strangler
- [x] `.\build.bat legacy_render` 绿（GL obj 含 `bind_rhi_present`）

## Non-goals（本 plan）

- 不改 scene3d / tool / ui / app
- 不把 `Backend::kGdi` 升为 bind 默认（仍属 design later）
- 不在 GL `Draw*` 中途打开 leftover_session（仍避免 mid-frame 与他 HWND Device 打架）
- 不 commit / 不开分支 / 不 CBM reindex
