<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI flatten + GdiPlayer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Flatten `impl/gdi` (drop `core/` + delete `gdiaux/`), split `paint/` into canvas/encode/player/gdiplus, introduce `GdiPlayer` as the single HDC play lane; converge immediate roads to dual GDI pens.

**Architecture:** Spec §GDI flatten + GdiPlayer in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md).

**Tech Stack:** C++23, Win32 GDI/GDI+, `legacy_render` / GN.

## Global Constraints

- Work on **`master`** only.
- Keep `SmtGdi*` / CreateDevice; never `render::rhi`.
- Do **not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `impl/gdi/host|worker|surface/` | Flat top (was under `core/`) |
| `paint/canvas/` | Orchestration |
| `paint/encode/` | Record |
| `paint/player/` | `GdiPlayer` play lane |
| `paint/gdiplus/` | Token + AA text |
| ~~`gdiaux/`~~ / ~~`core/`~~ | Deleted |

---

### Task 1: Flatten dirs + includes

- [x] Move `core/{host,worker,paint,surface}` → top; delete `gdiaux`.
- [x] Split `paint/` → `canvas/` `encode/` `player/` `gdiplus/`.
- [x] Bulk-fix includes / BUILD / tests / scene3d.

### Task 2: GdiPlayer + wire

- [x] `GdiPlayer`; encoder `replay` uses it.
- [x] Canvas immediate → `GdiPlayer`; roads → `road_polyline`.
- [x] Slim `GdiplusGraphics` (draw_string only).

### Task 3: Verify

- [x] `map_carto2d_test` / `gdi_compose_test` / `gdi_encode_test` green.
- [x] `legacy_render` + `gdi_map_paint_test` green (stale D3D/GL objs cleared).
- [x] Living § + README.

## Done when

- [x] Flat layout; paint subdirs; no gdiaux; GDI tests green; ABI unchanged.
