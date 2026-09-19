<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Hosts 3D DEM unify (Scene3dController)

**Status:** accepted  
**Date:** 2026-09-19

## Goal

Product hosts present DEM on the 3D tab via the same path:
`gis::DemRaster` → `World` → `Scene3dController` (`paint` / `present_gpu` / `paint_hud`).

## Scope (this delivery)

- **In scope (current):**
  - **Views** (`src/app/views/**`): `BrowserView` 3D tab — WinUI paint parity (`paint` when no shared frame / placeholder; `paint_hud` when FlyCube or ContentMapView DIB is live; `present_gpu` via `MapViewport::set_gpu_present`).
  - **CEF / Cs** (`src/app/cef/**`, `src/app/cs/**`): wire `CefMapSlot` / `SgHost` like WinUI `MapHost` (gestures + GN deps).
- **Out of scope:** `src/legacy/**` / leftover `SmartGis.exe`. Do **not** edit leftover for this delivery.
- Leftover option **2b** (controller presents DEM via FlyCube/GDI instead of GL) is **cancelled**.
- If leftover is ever done later, use option **2a** only (document-only; **do not implement now**):
  - Keep the GL shell (`Smt3DXView` / `SmtScene` render path).
  - Seed terrain from `gis::DemRaster` mesh into leftover buffers (per-scene / per-view).
  - No process-global `g_scene_dem` / `leftover_has_scene_dem()` short-circuit that skips per-view seed.

## Non-goals

- Implementing leftover 2a or 2b in this change.
- Atmosphere / ocean / cloud on leftover.
- Changing WinUI beyond shared API reuse (Views mirrors WinUI).
- Force-committing unless the user asks.

## Architecture

```
MapScene (optional land rings / extent)
  └─ Scene3dController::rebuild_local_mesh
       └─ gis::seed_china_dem_into_world
            ├─ Views / WinUI (reference + Views finish)
            └─ CefMapSlot / SgHost (CEF/Cs delivery)
```

### Views / WinUI

- Per-host `Scene3dController` + `MapScene`; on `kScene3d`:
  - no shared frame → `paint()` (GDI DEM wireframe via `seed_china_dem_into_world`);
  - FlyCube / live DIB → `paint_hud()`; FlyCube also calls `present_gpu`;
  - bind / re-bind extent on 3D tab activate; gestures → `apply_*` / `apply_draft`.

### CEF / Cs

- Mirror `MapHost` (WinUI): `Scene3dController` + `MapScene`; on `kScene3d`,
  no shared frame → `paint()`; with frame → `paint_hud()`; bind extent.
- Gestures: wheel / pan / orbit → `Scene3dController::apply_*` / `apply_draft`.
- GN: `//src/app/views:scene3d_controller` (+ `map_scene`).

### Leftover (deferred — 2a note only)

- Not part of this delivery. Future work, if any: **2a** as above; **2b** remains cancelled.

## Tests

- `scene3d_controller_test` stays green (seeded MapScene → `present_gpu` + GDI `paint`).
- Views `--self-test`: 3D tab orbit / trackball; optional FlyCube atmosphere path.
- CEF self-test / `sg_host_test`: 3D tab opens and is not solid placeholder-only when DEM path runs.

## Risks

- CEF Binary Dist missing on some machines → `build.bat cef` may be unavailable; Cs `sg_host_test` still validates native host wiring.
- Linking `scene3d_controller` into CEF/Cs increases deps (RHI / world); keep BUILD.gn honest.
- Preferring FlyCube at multi-viewport attach can hang DX12 — Views keeps ContentMapView default; FlyCube via env / showcase after shell is up.

## Related

- WinUI pattern: `src/app/winui/map_host.cc` (`scene3d_.paint` / `paint_hud`)
- Views: `src/app/views/browser_view.cc` (`wire_map_scene` / `switch_map_tab`)
- Controller: `src/app/views/scene3d_controller.{h,cc}`
- Plan: `docs/superpowers/plans/2026-09-19-leftover-scene3d-dem-unify.md`
