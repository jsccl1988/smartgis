<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# D3D leftover capability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Grow leftover **D3D11** `SmtD3DRenderDevice` under `rhi3d/impl/d3d/` (texture/FBO, font/frustum) while **D3D11 owns Present** on the scene3d HWND — not a FlyCube strangler.

**Architecture:** Spec §D3D leftover capability in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). **`Smt3DRenderDevice` + `"Direct3D"` factory unchanged.** Layout: `host/` · `resource/` · `paint/` · `caps/` · `ext/`. Distinct from `src/render/rhi` FlyCube.

**Tech Stack:** C++23, D3D11 + DXGI + D3DCompiler, `legacy_render` GN, hidden-HWND unit tests, `legacy_scene3d_shot_loop.py --d3d`.

## Global Constraints

- Work on **`master`** only; scope **`src/legacy/render/rhi3d/impl/d3d/`** (+ GN/tests).
- **`Init(HWND)`:** `bind_rhi_present` for Null recording only; **no FlyCube** on this HWND.
- Do **not** port full GLSL program stack; **VideoBuffer** and **shader manager** stay stub.
- New comments English; new-tree helpers **`snake_case`** where touched.
- No commit unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `host/render_device.*` | Device facade, swapchain, offscreen RT, capture |
| `host/device_present.cpp` | Init/Present/Swap/Clear lifecycle |
| `paint/draw.cpp` | DrawIndexed mesh, line strips, screen BGRA |
| `paint/matrix.cpp` | Matrix stack, **`GetFrustum`** |
| `paint/resources.cpp` | VB/IB, stub VideoBuffer/shaders |
| `resource/texture.cc` | Create/build/bind texture |
| `resource/frame_buffer.cc` | FBO attach/clear/unbind |
| `resource/font.cc` | GDI font slots + DrawText |
| `caps/`, `ext/` | Caps defaults, extension glue |
| `test/d3d_texture_test.cc` | Hidden HWND texture/FBO/frustum smoke |
| `README.md` | As-built layout + E2E command |

---

### T0: Baseline device + mesh present

- [x] Colocate layout under `host/` · `resource/buffer/` · `paint/` · `caps/` · `ext/`.
- [x] D3D11 device + swapchain + offscreen `color_tex_`; blit to swapchain on Present.
- [x] Lit **DrawIndexedPrimitives** (StereoTerrain XYZ+Normal+Diffuse) + **CaptureBgr24** staging.
- [x] Line strips + **DrawScreenBgra** for draped rivers / MapLabelBatch.
- [x] `SetViewport`: rasterizer only; RT resize only when size matches HWND client.

### T1: Texture + FBO

- [x] `resource/texture.cc` — create/build/bind/release; DXGI formats from leftover `TextureFormat`.
- [x] `resource/frame_buffer.cc` — create/destroy/bind/unbind; color (+ optional depth) attach; clear.
- [x] Wire sources in `BUILD.gn` `d3d_sources`.

### T2: Font + frustum

- [x] `resource/font.cc` — `CreateFont` / `DestroyFont`; world + screen **DrawText** via GDI bitmap path.
- [x] `paint/matrix.cpp` — **`GetFrustum`** matches leftover GL clip-plane extract.

### T3: Docs (living spec + as-built)

- [x] Append **§D3D leftover capability** on [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md).
- [x] This plan checklist (T0–T4).
- [x] Update [`../../../src/legacy/render/rhi3d/impl/d3d/README.md`](../../../src/legacy/render/rhi3d/impl/d3d/README.md).
- [x] Link plan from [`../README.md`](../README.md) RHI Active row (no new living row).

### T4: Verify

```bat
.\build.bat debug d3d_texture_test
.\build.bat debug legacy_app
py -3 testing\tools\legacy_scene3d_shot_loop.py --d3d --rounds 1
```

- [x] `d3d_texture_test` green (CreateTexture/Build/Bind + FBO clear/unbind + GetFrustum).
- [x] `legacy_scene3d_shot_loop.py --d3d` passes carto/engine gates (see `run_engine_shots.py` `legacy-scene3d-d3d`).
- [x] `leftover_record_test` green (no regression).
