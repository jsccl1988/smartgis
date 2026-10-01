<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Map browse forensic harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scripted 2D/3D map browse on Views + Legacy with optional window recording and timeline reports for hang / black-frame / lag / crash analysis.

**Architecture:** Extend §Harness suite loop + Interact DSL; add `record_hwnd` sidecar (ffmpeg or BMP burst); new/extended suites under `testing/tools/harness/`. Legacy uses OS inject + existing scene3d showcase linger — no new MFC product features.

**Tech Stack:** Python loop_runner / os_inject / Interact DSL; ffmpeg gdigrab (optional); SmartGisViews + SmartGis; captures under `out/<config>/captures/`.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Map browse forensic harness

## Global Constraints

- Gen roots `out/Debug` | `out/Release`; harness artifacts in `captures/`.
- Compile via `build.bat` + `SMARTGIS_BUILD_OWNER`; no bare ninja while lock held.
- Stay on `master`; no Qt; Legacy freeze except harness/path.
- Recording must not be a CI hard requirement without ffmpeg.

---

## Task 1: Record helper

**Files:**
- Create: `testing/tools/loop/record/hwnd.py`
- Modify: `testing/tools/loop/runner.py` (honor `SMT_HARNESS_RECORD` / suite env; attach `record_path` to report)

- [x] Implement find-window by title substring + start/stop record (ffmpeg `-f gdigrab` or PrintWindow BMP burst @ ~10 fps).
- [x] Wire runner: if record env set, start before exe, stop after; write path into report JSON.
- [x] Smoke: record 3s of notepad or Views HWND locally; file lands under `out/Debug/captures/`.

## Task 2: Views browse.3d + extend browse

**Files:**
- Create: `testing/tools/harness/shell/browse.3d/suite.json`, `browse.3d.il`, thin `*_loop.py`
- Modify: `testing/tools/harness/shell/browse/browse.il` (optional step timestamps / soft record note)
- Modify: showcase dispatch / `scenario_builtins` if new `--browse-showcase=3d` or argv needed

- [x] `.il`: switch 3D tab → orbit/drag/wheel → marks `browse3d-orbit-ok` / `browse3d-wheel-ok`.
- [x] suite.json: SmartGisViews, marks probes, optional record env.
- [x] `loop_runner --suite browse.3d --no-build` (after Views build) exit 0 or documented fail.

## Task 3: Legacy browse.2d / browse.3d

**Files:**
- Create: `testing/tools/harness/legacy/legacy.browse.2d/` (+ `.3d` or alias scene3d + inject)
- Modify: `src/legacy/app/README.md` (harness pointers only)

- [x] 2D: bring up SmartGis (Edit only), OS inject pan/wheel on map client; timeout = hang fail; record optional.
- [x] 3D: linger scene3d showcase HWND + inject or BMP gates + record.
- [x] Reports under `out/Debug/captures/`.

## Task 4: Docs + sample forensic run

**Files:**
- Modify: `docs/build/ui-testing.md` (L1′ browse forensic + record env)
- Modify: living § checklist when done

- [x] Document suite ids, `SMT_HARNESS_RECORD`, symptom → artifact mapping.
- [x] Produce one recorded sample each: Views 2D, Views 3D, Legacy 2D, Legacy 3D (local; wipe-ok under captures).

## Done bar

- [x] Four suite entries listed / runnable with `--no-build` when exes fresh.
- [x] Record path appears in report when env set; ffmpeg-missing falls back without hard fail.
- [x] Living § checklist boxes checked + ui-testing as-built note.
