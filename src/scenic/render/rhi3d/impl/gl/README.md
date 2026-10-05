<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GlRenderDevice (leftover OpenGL)

Windows **OpenGL** implementation of leftover `RenderDevice3d`, parallel to `rhi3d/impl/d3d/`. Distinct from modern `src/render/rhi` (FlyCube).

Living spec: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) §GL leftover capability · Plan: [`docs/superpowers/plans/2026-09-29-gl-leftover-capability.md`](../../../../../../docs/superpowers/plans/2026-09-29-gl-leftover-capability.md).

## FrameJob (leftover parallel P1)

Same contract as D3D for FrameJob/prep. **No** D3D deferred on GL — P3 is D3D-only (`RHI3D_D3D_DEFERRED` ignored). Living §: **§rhi3d leftover parallel frame**.

## Present

`GlRenderDevice::Init(HWND)` owns WGL + `SwapBuffers` on that HWND. Do **not** create FlyCube on this HWND. The former `bind_rhi_present` / leftover_session strangler was removed.

## Factory (ABI unchanged)

| API string | Export | Class | DLL |
| --- | --- | --- | --- |
| `"OpenGL"` | `Create3DRenderDevice` | `GlRenderDevice` | `scenic_render_gl` |
| `"Direct3D"` | `CreateD3DRenderDevice` | `D3dRenderDevice` | `scenic_render_d3d` |

Release uses `Release3DRenderDevice` in the same backend DLL that created the device.

`GetBaseApi()` reports `RA_OPENGL`.

## Directory layout

Colocated units under `src/scenic/render/rhi3d/impl/gl/` (mirrors D3D `host/` · `resource/` · `paint/`):

| Directory | Contents |
| --- | --- |
| **`host/`** | `render_device.*` — facade; `device_present.cpp` — Begin/End/SwapBuffers/Draw*/DrawText |
| **`resource/`** | `buffer/` VB·IB; `texture.cpp`; `frame_buffer.cpp`; `font.cpp`; `text/` glyph list |
| **`paint/`** | `fast_draw` / effect / misc / matrix / shader / vba / vbo; `states_manager.*` (tiny util accessors are inline on `host/render_device.h`) |
| **`caps/`** | `device_caps.h` (header-only) |
| **`ext/`** | `*_func*.h` header-only stubs + `*_imp` loaders; `ext_interface.cpp` (DllMain / factory) |
| **`test/`** | `map_paint_test.cc` (shown HWND + china_plp); `gl_texture_test.cc` (hidden HWND smoke) |

GN: `//src/scenic/render/rhi3d/impl/gl:gl_sources` → `scenic_render_gl`.

## File naming (snake_case)

Mechanical rename to match D3D/GDI leftover stems. **ABI types/exports unchanged** (`GlRenderDevice`, `Create3DRenderDevice`, …).

| Old | New |
| --- | --- |
| `host/3drenderdevice.*` | `host/render_device.*` |
| `host/rdev_render.cpp` | `host/device_present.cpp` |
| `paint/rdev_*.cpp` | `paint/{effect,fast_draw,misc,matrix,shader,util,vba,vbo}.cpp` |
| `paint/statesmanager.*` | `paint/states_manager.*` |
| `caps/devicecaps.*` | `caps/device_caps.*` |
| `ext/extinterface.cpp` | `ext/ext_interface.cpp` |
| `ext/{fbo,vbo,shader,mipmap,vsync,multitexture}func*.{h,cpp}` | `ext/*_func*.{h,cpp}` (also fix typo `mutitexturefunc` → `multitexture_func`) |
| `resource/rdev_{textures,fbo,font}.cpp` | `resource/{texture,frame_buffer,font}.cpp` |
| `resource/buffer/{index,vertex}buffer.*` | `resource/buffer/{index,vertex}_buffer.*` |

Intentionally kept: `GLRenderDevice.mak` / `.plg` (legacy VS leftover stubs, not in GN).

## Implemented (as-built)

GL was already ahead of D3D on texture/FBO/font/frustum. This slice is primarily **layout alignment**, plus a few correctness fixes found by the new smoke test.

- **Host:** WGL context Init/Destroy/Release; `SwapBuffers` present.
- **Texture / FBO:** `CreateTexture` / `BuildTexture` / `BindTexture` / `GenerateMipmap`; FBO create/attach/bind/clear/unbind (`resource/{texture,frame_buffer}.cpp` + `ext/`). **Fixed:** `ConvertRenderBufferSlot` now accepts `COLOR_ATTACHMENT0` (was `index > 0`); attach binds the target FBO first.
- **Font / frustum:** GDI bitmap font lists (`resource/text` + `font.cpp`); world + screen `DrawText`; `GetFrustum` from GL modelview × projection (`paint/misc.cpp`). **Fixed:** CreateFont failure path deletes the helper and releases HDC.
- **Draw:** immediate/VBO paths, lit mesh, fastdraw, state manager.

**Still stub / deferred:** full programmable shader manager enrichment beyond existing GL extension path; no FlyCube on this HWND; no rewrite of public `Smt_*` ABI.

## E2E

```bat
py -3 testing\tools\harness\legacy\legacy.scene3d.china\legacy_scene3d_china_loop.py --rounds 1
```

Omit `--d3d` (or set `STEREO_API=OpenGL`) for GL. Product may default to D3D11 elsewhere; this loop defaults to GL.

**Unit smoke:**

```bat
.\build.bat debug gl_texture_test
.\build.bat debug gl_map_paint_test
```
