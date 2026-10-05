<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# D3dRenderDevice (leftover D3D11)

Windows **D3D11** implementation of leftover `RenderDevice3d`, parallel to `rhi3d/impl/gl/`. Chosen over D3D12 to match the product D3D11 stack and avoid resurrecting deleted D3D9/D3DX. Distinct from modern `src/render/rhi` (Vista).

Living spec: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) §D3D leftover capability · Plan: [`docs/superpowers/plans/2026-09-29-d3d-leftover-capability.md`](../../../../../../docs/superpowers/plans/2026-09-29-d3d-leftover-capability.md).

## FrameJob (leftover parallel P1)

Stereo HWND present can run on a serial FrameJob worker (`Rhi3dFrameScheduler` under `rhi3d/impl/common/frame/`). Default **on**; set `RHI3D_FRAME_JOB=0` for sync present on the caller thread. `stereo_hwnd_present` returns after submit; `capture` / `blit` wait for publish. Destroy never joins the worker. Living §: **§rhi3d leftover parallel frame**.

**P2 CPU prep:** `Rhi3dPrepRunner` (`RHI3D_PREP_PARALLEL=0` → N=1; else clamp 2–4) parallelizes AABB-in-frustum before serial draw. Workers must not call GL/D3D.

**P3 deferred (D3D only):** `RHI3D_D3D_DEFERRED` default **on** (`=0` serial). `CreateDeferredContext` per worker; TLS `active_context` / per-slot mesh CB; scene/octree partition visible objects → FinishCommandList → Execute on immediate. GL ignores this env.

## Present

`D3dRenderDevice::Init(HWND)` owns D3D11 `IDXGISwapChain::Present` on that HWND. Do **not** create Vista on this HWND. The former `bind_rhi_present` / leftover_session strangler was removed.

## Factory (ABI unchanged)

| API string | Export | Class |
| --- | --- | --- |
| `"OpenGL"` | `Create3DRenderDevice` | `GlRenderDevice` |
| `"Direct3D"` | `CreateD3DRenderDevice` | `D3dRenderDevice` |

Release uses shared `Release3DRenderDevice` in the same `scenic_impl` DLL.

`GetBaseApi()` reports leftover enum `RA_D3D09` (historical name for the non-GL slot); the runtime backend is **D3D11**.

## Directory layout

Colocated units under `src/scenic/render/rhi3d/impl/d3d/`:

| Directory | Contents |
| --- | --- |
| **`host/`** | `render_device.*` — facade; `device_present.cpp` — Init/Destroy/Present/Swap/Clear/capture |
| **`resource/`** | `buffer/` VB·IB (`index_buffer` / `vertex_buffer`); **`texture.cc`**; **`frame_buffer.cc`**; **`font.cc`** |
| **`paint/`** | `draw.cpp` draw paths; `matrix.cpp` matrices + frustum; `resources.cpp` buffers + stub shaders/VideoBuffer; `states_manager.*` |
| **`caps/`** | `device_caps.*` |
| **`ext/`** | `ext_interface.cpp` |
| **`test/`** | `d3d_texture_test.cc` (hidden HWND smoke) |

GN: `//src/scenic/render/rhi3d/impl/d3d:d3d_sources` → `scenic_impl`.

## File naming (snake_case)

Mechanical rename to match GDI leftover / repo-global stems. **ABI types/exports unchanged** (`D3dRenderDevice`, `CreateD3DRenderDevice`, …).

| Old | New |
| --- | --- |
| `host/3drenderdevice.*` | `host/render_device.*` |
| `host/rdev_render.cpp` | `host/device_present.cpp` |
| `paint/rdev_draw.cpp` | `paint/draw.cpp` |
| `paint/rdev_mtx.cpp` | `paint/matrix.cpp` |
| `paint/rdev_resources.cpp` | `paint/resources.cpp` |
| `paint/statesmanager.*` | `paint/states_manager.*` |
| `caps/devicecaps.*` | `caps/device_caps.*` |
| `ext/extinterface.cpp` | `ext/ext_interface.cpp` |
| `resource/buffer/{index,vertex}buffer.*` | `resource/buffer/{index,vertex}_buffer.*` |
| `resource/framebuffer.cc` | `resource/frame_buffer.cc` |

Public headers under `rhi3d/public/` follow the same snake_case map (`3drenderdevice.h` → `render_device.h`, …).

## Implemented (as-built)

**T0 — device / swapchain / mesh / capture**

- D3D11 device + DXGI swapchain; offscreen **`color_tex_`** RT (Clear/Draw), blit to swapchain on Present.
- Init/Destroy/Release, Begin/End/Swap/Clear, CPU matrix stack (heap-backed), system-memory VB/IB, state-manager cache, caps defaults.
- **DEM Terrain DrawIndexedPrimitives** — XYZ+Normal+Diffuse lit mesh (HLSL compile-at-init, not full GLSL program port).
- Staging **`CaptureBgr24`** (pre-Present snapshot).
- Line strips + **DrawScreenBgra** (MapLabelBatch / draped rivers).

**T1 — texture / FBO**

- **Texture** create, build, bind, release (`resource/texture.cc`).
- **Frame buffer** create/destroy, attach color (optional depth), bind/unbind, clear (`resource/frame_buffer.cc`).

**T2 — font / frustum**

- **GDI font** slots — `CreateFont` / `DestroyFont`; **DrawText** (world + screen) via bitmap upload (`resource/font.cc`).
- **`GetFrustum`** — GL-compatible six-plane extract from modelview × projection (`paint/matrix.cpp`).

**`SetViewport`:** rasterizer viewport only (matches GL). Framebuffer resize happens only when the requested size equals the HWND client size — mid-frame leftover 120×120 viewports must not recreate color targets.

**Still stub / deferred:** **`VideoBuffer`** GPU bind/update/map; **`Shader` / `Program`** manager (no full GLSL port); **`Transform2DTo3D`** and related pick helpers return failure until ported.

## E2E

```bat
py -3 testing\tools\case\legacy_scene3d_shot_loop.py --d3d --rounds 1
```

Also wired in `testing/tools/case/run_engine_shots.py` as **`legacy-scene3d-d3d`**.

Product default is **D3D11**. `--d3d` sets `STEREO_API=Direct3D` / `SCENE3D_SHOWCASE_D3D=1`; omit `--d3d` (or set `STEREO_API=OpenGL`) for GL.

**Runtime:** GN copies `d3dcompiler_47.dll` (Windows Kits Redist) into `out/Debug|Release` via `//src/scenic:d3dcompiler_runtime_dll` — required by `scenic_impl` when the D3D DrawIndexed path is linked.

**Unit smoke:**

```bat
.\build.bat debug d3d_texture_test
```

**Lighting (DEM Terrain):** mesh PS mirrors leftover GL `setup_device_lights` + `COLOR_MATERIAL`: ambient 1.0 and two white directionals from `(1,1,1)` (identity-MV bake), normals in object space — see `paint/draw.cpp`.
