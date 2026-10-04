<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness visual review closed-loop — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Codify showcase visual QA (capture → Agent bug list → human confirm → fix → re-score) into harness contracts, loop artifacts, and a Cursor skill.

**Architecture:** Optional `visual_review` on `suite.json`; `testing/tools/loop/review/` converts BMP→inspect PNG and emits `*_visual_review.json`; `loop_runner --review-prep` is a one-shot prep path. Agent/human workflow lives in `.cursor/skills/harness-visual-review/SKILL.md`. No default `te` gate.

**Tech Stack:** Python 3 (`loop_runner` / Pillow or WIC fallback for PNG); existing `score_bmp`; Cursor skill markdown.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Visual review closed-loop

## Global Constraints

- Gen roots `out/Debug` | `out/Release`; artifacts under `out/<config>/captures/`.
- Stay on `master`; no Qt; no new pixel goldens for map/GPU.
- Not wired into default `build.bat te`.
- Do not invent a second dated design twin — revise living § only.
- Compile via `build.bat` when a suite run needs a rebuild.

---

## File map

| Path | Role |
| --- | --- |
| `testing/tools/loop/review/inspect_png.py` | BMP → `*.inspect.png` |
| `testing/tools/loop/review/emit_review.py` | Build/write `*_visual_review.json` stub |
| `testing/tools/loop/suite.py` | Parse optional `visual_review` |
| `testing/tools/loop/runner.py` | After bmp score: inspect + emit; honor `--review-prep` |
| `testing/tools/loop_runner.py` | CLI `--review-prep` |
| `testing/tools/harness/**/suite.json` | Seed `visual_review` on priority suites |
| `.cursor/skills/harness-visual-review/SKILL.md` | Agent closed-loop protocol |
| `docs/superpowers/ui-testing.md` | As-built short section |

---

## Task 1: review package (inspect + emit)

**Files:**
- Create: `testing/tools/loop/review/__init__.py`
- Create: `testing/tools/loop/review/inspect_png.py`
- Create: `testing/tools/loop/review/emit_review.py`

- [x] `bmp_to_inspect_png(bmp: Path) -> Path` writes sibling `*.inspect.png` (Pillow preferred; document WIC/`magick` fallback if Pillow missing).
- [x] `emit_visual_review(suite, config, score: dict | None, status="pending") -> Path` writes captures `/{suite_id with '.'→'_'}_visual_review.json` (or next to bmp stem) with fields from living §.
- [x] Unit-style smoke: convert a tiny fixture BMP or skip-if-no-Pillow with clear message.

---

## Task 2: suite contract + runner wire

**Files:**
- Modify: `testing/tools/loop/suite.py`
- Modify: `testing/tools/loop/runner.py`
- Modify: `testing/tools/loop_runner.py`

- [x] Add frozen `VisualReview` dataclass (`enabled`, `checklist`, `expect_notes`); parse from `suite.json`.
- [x] Default: if `bmp` present and no block → reviewable (`enabled` effective true for `--review-prep`).
- [x] After successful/attempted bmp score in `_score_round` / report path: when reviewable, write inspect PNG + review JSON (include score snapshot).
- [x] `--review-prep`: force rounds=1, run suite (or reuse last bmp with `--bmp` + emit only), print paths to inspect + review JSON; exit non-zero only on run/score hard fail (not on “bugs pending”).
- [x] Smoke: `py -3 testing/tools/loop_runner.py --suite map2d.china --review-prep --no-build` (or stormsurge) produces inspect + JSON under `out/Debug/captures/`.

---

## Task 3: seed priority suites

**Files:**
- Modify: `testing/tools/harness/plugin/plugin.stormsurge/suite.json`
- Modify: `testing/tools/harness/atmosphere/atmosphere.full/suite.json`
- Modify: `testing/tools/harness/map2d/map2d.china/suite.json`
- Modify: primary `plugin.*` showcase `suite.json` files that already define `bmp` (world3d, mine, geochem, flood, traffic, orthogrid, orthogrid3d, print, report as present)

- [x] Add `"visual_review": { "enabled": true, "checklist": [...], "expect_notes": "..." }` with product-specific checklist hints (e.g. stormsurge: water contrast; atmosphere: sky/cloud; map2d: coast/roads).
- [x] Do not require every harness suite in one change — priority list from living § is enough for v1.

---

## Task 4: Cursor skill + as-built docs

**Files:**
- Create: `.cursor/skills/harness-visual-review/SKILL.md`
- Modify: `docs/superpowers/ui-testing.md`
- Modify: `docs/superpowers/README.md` (Active plans column mention if needed)

- [x] Skill steps: `--review-prep` or open existing capture → `Read` inspect PNG → numbered bug table → **wait for human confirm** → fix → `build.bat debug …` → re-run → re-score → update review JSON status / tighten `score_id` if gate missed the bug → split crash agent when EXIT heap/AV.
- [x] ui-testing: short §Visual review closed-loop with CLI examples + link to living §.
- [x] Refresh README “最后更新” / plans bullet for visual-review.

---

## Done bar (Wave1)

- [x] `--review-prep` on at least one priority suite writes inspect PNG + `pending` review JSON.
- [x] Skill + living § + ui-testing as-built agree on the state machine.
- [x] No default `te` coupling.

---

## Wave2: browse/ui/legacy checklist + real review runs

**Goal:** Expand explicit `visual_review` beyond plugin priority list; run Agent vision review on `legacy.browse.2d` + `map2d.china` and stop for human confirm before fixes.

### Task 5: seed Wave2 suite contracts

**Files:** `testing/tools/harness/{legacy,ui,atmosphere,map2d}/**/suite.json` (bmp suites only)

- [x] `legacy.browse.2d` / `legacy.browse.3d` — zoom/pan/black-frame checklist.
- [x] `ui.shell` / `ui.catalog` / `ui.data` / `ui.scene` / `ui.interact` / `ui.interact.os` — chrome readability / collapse / dark theme.
- [x] `atmosphere.legacy`, `legacy.map2d.china`, `legacy.scene3d.china` (+ `.d3d`), `map2d.orthogrid`.
- [x] Skip marks-only suites (`browse`, `browse.3d`, `plugin.report`, `ui.interact.smoke` / `.combo`).

### Task 6: real `--review-prep` + bug tables

- [x] `py -3 testing/tools/loop_runner.py --suite legacy.browse.2d --review-prep --force-run --no-build` (build `SmartGis` only if exe missing).
- [x] Same for `map2d.china` (`SmartGisViews.exe`) — Wave2 used force-run; exit 0 + bmp_ok after score land_like gates.
- [x] Agent `Read` each `*.inspect.png` → numbered bug table (severity; product vs gate gap).
- [x] **Human confirm** (user: 修复所有) → fix.
- [x] On confirmed visual miss: tighten `score_id` / `zoom_gate` in same change when practical; update review JSON status.

### Task 7: docs refresh

- [x] Living §Visual review decisions 11–12 + Wave2 checklist row.
- [x] `docs/superpowers/ui-testing.md` Wave2 one-liner (suite list + review-prep ids).
