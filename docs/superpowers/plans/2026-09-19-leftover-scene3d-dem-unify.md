<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Hosts 3D DEM unify Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Product hosts present DEM via `Scene3dController` (Views / WinUI parity), not placeholder-only blit.

**Architecture:** WinUI paint parity — per-host `Scene3dController` + `MapScene`; `paint` when no GPU frame, `paint_hud` when a shared frame / FlyCube present is live; `present_gpu` on FlyCube. Gestures forward to the controller.

**Scope change (normative):** This delivery = **Views + CEF + Cs**. Leftover / `src/legacy/**` is **ignored**. Leftover option **2b** is **cancelled**. If leftover is ever done later, use option **2a** (GL shell kept; terrain from `gis::DemRaster` mesh into leftover buffers; per-scene seed; no global-dem short-circuit) — **do not implement 2a now**.

**Tech Stack:** C++23, GN, `app::Scene3dController`, `gis::seed_china_dem_into_world`, CEF HWND island, Cs native host, SmartGisViews.

## Global Constraints

- Stay on **master**; do not create branches.
- Do **not** commit unless the user explicitly asks.
- Comments in English; user-facing docs Chinese OK.
- Copyright year **2026** The Mogu Authors on new/touched headers.
- New-tree functions `snake_case`.
- No Qt. Match local tree style.

## File map

| Area | Files |
|------|--------|
| Spec | `docs/superpowers/specs/2026-09-19-leftover-scene3d-dem-unify-design.md` |
| Controller (shared) | `src/app/views/scene3d_controller.{h,cc}` |
| Views (this finish) | `src/app/views/browser_view.{cc,h}`, `main.cc`, `BUILD.gn` |
| WinUI reference | `src/app/winui/map_host.cc` / `.h` |
| Task A leftover | **cancelled / deferred** — was `src/legacy/**`; do not touch |
| Task B CEF | `src/app/cef/cef_map_slot.{cc,h}`, `src/app/cef/BUILD.gn`, maybe `chrome_bridge` |
| Task B Cs | `src/app/cs/native/sg_host.{cc,h}`, `src/app/cs/BUILD.gn` |
| Task V Views | `src/app/views/**` — paint / extent / gestures / `present_gpu` |

---

## Task A: Leftover SmartGis 3D → Scene3dController — CANCELLED / DEFERRED

**Status:** cancelled for this delivery (leftover ignored). Option **2b** cancelled. Future leftover work, if any → option **2a** only (document in spec; not implemented here).

**Paths only (historical):** `src/legacy/**` — **do not edit**.

- [x] ~~A1–A6~~ **Cancelled** — not in this delivery.

**Done when:** N/A for this delivery.

---

## Task V: Views → Scene3dController (WinUI paint parity) — ACTIVE

**Paths only:** `src/app/views/**` (+ this plan/spec).

- [x] **V1** Audit `BrowserView` vs WinUI `MapHost` (`paint` / `paint_hud` / `present_gpu` / extent / gestures).
- [x] **V2** Fix 3D overlay: no shared frame → `paint()` DEM; FlyCube / live DIB → `paint_hud()`; correct client width/height.
- [x] **V3** On 3D tab activate: re-`bind_contents`, `push_shared_extent`, invalidate.
- [x] **V4** Extend `scene3d_controller_test` for seeded MapScene → `present_gpu` + GDI `paint`.
- [x] **V5** Build/run `scene3d_controller_test` (and Views self-test if buildable). Do not commit.

**Done when:** Views 3D tab uses the same DEM controller path as WinUI; no edits outside Views (+ docs).

---

## Task B: CEF + Cs → Scene3dController (WinUI parity) — ACTIVE

**Paths only:** `src/app/cef/**`, `src/app/cs/**`. Do **not** edit `src/legacy/**`. Prefer not editing `src/app/views/**` except shared controller APIs already owned by Task V.

- [x] **B1** `sg_host_test` asserts 3D show_kind + trackball/wheel; CEF self-test covers scene tab + orbit.
- [x] **B2** `CefMapSlot`: `Scene3dController` + `MapScene`; `kScene3d` → `paint` / `paint_hud`; GN deps.
- [x] **B3** `SgHost`: same WinUI paint path; GN deps on `scene3d_controller` / `map_scene`.
- [x] **B4** Gestures: CEF wheel/pan/draft + Cs `apply_pointer` → controller.
- [x] **B5** `build.bat cs` / `build.bat cef` green; `sg_host_test` / `chrome_bridge_test` / `SmartGisCef --self-test` exit 0. Do not commit.

**Done when:** CEF and Cs 3D slots present DEM like WinUI when GPU frame missing/present; no edits outside Task B paths (+ docs).

---

## Integration (parent agent)

- [ ] Confirm no leftover DEM work; Views V5 still open for parent/Views agent.
- [x] Task B: `build.bat cef` / `build.bat cs` / `sg_host_test` / CEF `--self-test` green (2026-09-19).
- [x] Spec/plan on disk; no commit unless user asks.
- Note: mechanical `//src/sdb`→`//src/base|gis` deps in `src/legacy/render/scene3d/BUILD.gn` only to unblock `gn gen` (not leftover 2a/2b).
