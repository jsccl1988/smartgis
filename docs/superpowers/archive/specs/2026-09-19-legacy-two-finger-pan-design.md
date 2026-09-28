<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) — §two-finger pan (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


Status: superseded (2026-09-28 merge B)

# Legacy two-finger pan (left/right)

## Goal

`SmartGis.exe` leftover map views (`src/legacy/ui/xview`) support the same two-finger horizontal pan contract as Views: touch `GID_PAN` and trackpad `WM_MOUSEHWHEEL`.

## Non-goals

- Do not route legacy UI through `app/views/MapHwndGestures` (wrong dependency direction).
- Do not change `Workspace` / `kTouchPan` draft semantics (already correct).

## Input paths

| Source | Message | Chrome mapping | Downstream |
| --- | --- | --- | --- |
| Trackpad two-finger horizontal | `WM_MOUSEHWHEEL` | `kWheel` + `kHorizontalWheel` | always-on → `kTouchPan` Draft → ViewCtrl / 3DViewCtrl `apply_draft` |
| Touch two-finger drag | `WM_GESTURE` `GID_PAN` | multitouch `InputEvent` (`pointer_count >= 2`) | `view.pan` / `view3d.*` → `kTouchPan` Draft |
| Pinch zoom | `GID_ZOOM` (+ existing pointer pinch when not panning) | `kWheel` via `scale_to_wheel_delta` | wheel Draft |
| Pointer during `GID_PAN` | `WM_POINTER*` | swallow | avoid double pan with midpoint / pinch |

`SetGestureConfig` enables `GID_ZOOM` + `GID_PAN` (no single-finger pan flags).

## Files

- `src/legacy/ui/xview/view_shell.cc` / `.h` — primary wiring
- `src/legacy/ui/xview/view_3d.cpp` / `xview.cpp` — pass `HWND`; treat `WM_MOUSEHWHEEL` like wheel for return

## Arbitration

While a `GID_PAN` session is active, do not feed `WM_POINTER*` into pinch or midpoint pan. Pinch remains `GID_ZOOM` and pointer distance change when not panning.
