<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# UI Visual Forensics (A+C) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship **Scheme 1** UI visual forensics — **Mode A** failure dumps under `out/ui_forensics/<run_id>/` and optional **Mode C** Python driver — per [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §UI visual forensics (A+C).

**Architecture:** Existing semantic / `layout_check` gates stay authoritative. On failure (or when `SMT_UI_FORENSICS=1`), shell offscreen WIC capture writes `frame_NNNN.png`, `manifest.json`, and `layout_issues.txt`. Mode C reuses DebugAgent `ui.*` for live `--record-all` and offline `--analyze` heuristics (AABB overlap, TabStrip, Ambox y-spacing, Gantt lane gaps). No map GPU pixel goldens.

**Tech Stack:** C++23 test runners, Views shell capture, WIC, DebugAgent NDJSON, Python 3 at `tools/debug/scripts/ui_visual_forensics.py`.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- **No Qt**; **Views + Skia** only.
- **Do not** block default **`build.bat te`** on forensics or Mode C.
- **Do not** add map viewport pixels to L2 or forensics goldens.
- Copyright year **2026**; English comments in new/changed source.
- As-built: [`../ui-testing.md`](../ui-testing.md) **L1c**.

## File map (expected)

| Path | Role |
| --- | --- |
| Shared forensics hook (test / self-test / pixel runners) | Mode A: allocate `run_id`, write trio under `out/ui_forensics/` |
| `tools/debug/scripts/ui_visual_forensics.py` | Mode C: `--record`, `--record-all`, `--analyze` |
| `docs/superpowers/ui-testing.md` | L1c row + how-to (this plan lands doc first) |
| Existing: `layout_check.h`, capture/WIC helpers, DebugAgent `ui.*` | Reuse only — no duplicate widget kit |

---

### Task 1: Mode A artifact writer

**Behavior:**
- On semantic / `layout_check` failure from L0, L1, L1′, or L2 runner, create `out/ui_forensics/<run_id>/`.
- Write `layout_issues.txt` from violation collector output.
- Write `manifest.json` (run id, exe, scenario, frame index, optional layout metrics).
- Capture shell offscreen frames as `frame_NNNN.png` (no map GPU bitmap).

- [x] **Step 1:** Define stable `run_id` generation and directory layout (document in manifest schema).
- [x] **Step 2:** Hook L0 `views_unittests` failure path (minimal: one representative gate).
- [ ] **Step 3:** Hook L1 `views_interactive_tests` failure path.
- [x] **Step 4:** Hook L1′ `SmartGisViews.exe --self-test` failure path.
- [ ] **Step 5:** Hook L2 `views_pixel_tests` failure path (shell PNG only; no map frame).
- [x] **Step 6:** Implement `SMT_UI_FORENSICS=1` pass-through dump even on success (dev only).

---

### Task 2: Manifest metrics for analyze

- [x] **Step 1:** TabStrip — cell bounds vs text width in manifest when Catalog tabs visible.
- [x] **Step 2:** Ambox Tools — button AABB list with y coordinates.
- [x] **Step 3:** DiagnosticTools CPU Gantt — lane bounds / min gap when panel open.
- [ ] **Step 4:** Map/Data/3D switch — record semantic mark strings only (no map pixels).

---

### Task 3: `ui_visual_forensics.py` (Mode C)

- [x] **Step 1:** Scaffold script with copyright 2026; argparse for `--record`, `--record-all`, `--analyze <dir>`.
- [ ] **Step 2:** `--record` — discover latest or given forensics dir; fail-only append via Agent (align with last test run).
- [x] **Step 3:** `--record-all` — live Agent capture loop (document env / discovery file parity with `ui_smoke.py`).
- [x] **Step 4:** `--analyze` — frame-by-frame: sibling AABB overlap; TabStrip cell vs text width; Ambox button y spacing; Gantt lane min gap when manifest metrics present.
- [x] **Step 5:** Exit non-zero on analyze violations; human-readable report to stdout.

---

### Task 4: Scenario matrix coverage

- [x] Catalog Layers / Sources / Maps — no label pile-up (capture + analyze).
- [x] Ambox Tools — no y-collapse between tool buttons.
- [x] DiagnosticTools CPU Gantt — lanes spaced (min gap heuristic).
- [ ] Map ↔ Data ↔ 3D — `map-frame-ok` / `scene-frame-ok` marks without map pixel golden.

---

### Task 5: Docs + GN policy

- [x] **Step 1:** Living §UI visual forensics (A+C) in views-desktop-shell spec.
- [x] **Step 2:** L1c row + how-to in `docs/superpowers/ui-testing.md`.
- [x] **Step 3:** This plan + Active table link in `docs/superpowers/README.md`.
- [x] **Step 4:** Confirm Mode C script **not** added to default `//:test_all` / `build.bat te`.
- [ ] **Step 5:** Optional CI job doc line (forensics artifact upload on failure only).

---

## Verification

```bat
REM Failure path (after implementation): force a layout_check fail → dir appears
build.bat debug views_unittests
REM Dev dump on pass
set SMT_UI_FORENSICS=1
out\views_unittests.exe
REM Mode C (optional, not te)
python tools\debug\scripts\ui_visual_forensics.py --analyze out\ui_forensics\<run_id>
```

Default **`build.bat te`** must stay green **without** running Mode C or requiring forensics artifacts.
