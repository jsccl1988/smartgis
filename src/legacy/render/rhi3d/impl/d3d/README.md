<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmtD3DRenderDevice (leftover D3D11)

Windows **D3D11** implementation of leftover `Smt3DRenderDevice`, parallel to `rhi/impl/gl/`. Chosen over D3D12 to match the product D3D11 stack and avoid resurrecting deleted D3D9/D3DX. Distinct from modern `src/render/rhi` (FlyCube).

## Present strangler

`SmtD3DRenderDevice::Init(HWND)` calls `render::bind_rhi_present(hWnd)` (Null recording). **This HWND’s present is owned by D3D11 `IDXGISwapChain::Present`** — do not create FlyCube here.

## Factory

| API string | Export | Class |
| --- | --- | --- |
| `"OpenGL"` | `Create3DRenderDevice` | `SmtGLRenderDevice` |
| `"Direct3D"` | `CreateD3DRenderDevice` | `SmtD3DRenderDevice` |

Release uses shared `Release3DRenderDevice` in the same `legacy_render` DLL.

`GetBaseApi()` reports leftover enum `RA_D3D09` (historical name for the non-GL slot); the runtime backend is **D3D11**.

## v1 scope

**Implemented:** device + swapchain + RTV/DSV, Init/Destroy/Release, Begin/End/Swap/Clear/viewport resize, CPU matrix stack, system-memory VB/IB, state-manager cache, caps defaults.

**Deferred:** DrawPrimitives / shaders / textures / FBO / font / video-buffer / frustum extract (return failure / null until ported).

## GN

`//src/legacy/render/rhi3d/impl/d3d:d3d_sources` → `legacy_render` (`d3d11.lib`, `dxgi.lib`).
