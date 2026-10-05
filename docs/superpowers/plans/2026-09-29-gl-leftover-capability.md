<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GL leftover capability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align leftover **OpenGL** `SmtGLRenderDevice` under `rhi3d/impl/gl/` with D3D/GDI directory seams (`host/` · `resource/` · `paint/`), keep **GL `SwapBuffers`** as present owner, and verify texture/FBO/font/frustum — not a Vista strangler.

**Architecture:** Spec §GL leftover capability in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (sibling of §D3D leftover capability). **`Smt3DRenderDevice` + `"OpenGL"` factory unchanged.** Distinct from `src/render/rhi` Vista. Do **not** edit `impl/d3d/`.

**Tech Stack:** C++23, WGL/OpenGL + GLU, `legacy_render` GN, hidden-HWND unit tests, `legacy_scene3d_shot_loop.py` (GL default).

## Global Constraints

- Work on **`master`** only; scope **`src/legacy/render/rhi3d/impl/gl/`** (+ GN/tests + living docs that mention both).
- **`Init(HWND)`:** `bind_rhi_present` for Null recording only; **no Vista** on this HWND.
- Enrich only where clearly broken or thinner than public ABI; prefer mechanical layout move.
- New comments English; new-tree helpers **`snake_case`** where touched.
- No commit unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `host/render_device.*` | Device facade, WGL Init/Destroy |
| `host/device_present.cpp` | Begin/End/SwapBuffers/Draw*/DrawText |
| `resource/buffer/` | VB/IB (`index_buffer` / `vertex_buffer`) |
| `resource/texture.cpp` | Create/build/bind texture |
| `resource/frame_buffer.cpp` | FBO attach/clear/unbind |
| `resource/font.cpp` + `resource/text/` | GDI font slots + DrawText helper |
| `paint/` | matrix/misc/fast_draw/effect/shader/util/vba/vbo + states_manager |
| `caps/`, `ext/` | Caps + extension loaders |
| `test/gl_texture_test.cc` | Hidden HWND texture/FBO/font/frustum smoke |
| `README.md` | As-built layout + E2E command |

---

### T0: Path scaffold (host / resource / paint)

- [x] `git mv` `device/` → `host/` + `paint/`; `buffer/` → `resource/buffer/`; `text/` → `resource/text/`; texture/FBO/font TUs under `resource/`.
- [x] Update `BUILD.gn` sources + in-tree `#include` paths.
- [x] Behavior-preserving compile of `gl_sources` / `legacy_render`.

### T1: Capability audit (texture / FBO)

- [x] Confirm `CreateTexture` / `BuildTexture` / `BindTexture` / `GenerateMipmap` still wired after move.
- [x] Confirm FBO create/attach/bind/clear/unbind still wired (`resource/frame_buffer.cpp` + `ext/`).
- [x] No stub fill required — GL already complete vs public ABI for this track.

### T2: Font / frustum regression

- [x] `CreateFont` / `DrawText` / `GetFrustum` still wired; fix CreateFont failure-path leak (`resource/font.cpp`).
- [x] `gl_texture_test` exercises font + frustum + textured DrawIndexed.

### T3: Docs (living spec + as-built)

- [x] Append **§GL leftover capability** on [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (do not rewrite mid-flight §D3D).
- [x] This plan checklist (T0–T4).
- [x] Update [`../../../src/legacy/render/rhi3d/impl/gl/README.md`](../../../src/legacy/render/rhi3d/impl/gl/README.md).
- [x] Link plan from [`../README.md`](../README.md) RHI Active row (no new living row).

### T4: Verify

```bat
.\build.bat debug gl_texture_test
.\build.bat debug legacy_render
py -3 testing\tools\legacy_scene3d_shot_loop.py --rounds 1
```

- [x] `gl_texture_test` green (CreateTexture/Build/Bind + FBO clear/unbind + CreateFont/GetFrustum).
- [x] `legacy_scene3d_shot_loop.py --rounds 1` (GL, no `--d3d`) PASS visual gates (`ok: true`). Full `build.bat debug SmartGis` may still hit unrelated `ui_legacy` MFC LNK2005 from parallel work — verify used `--no-build` once `legacy_render` + GL were green.
- [x] `legacy_render` + `gl_texture_test` debug build green.
