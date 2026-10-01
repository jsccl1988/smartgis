<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness visual review (closed-loop)

Use when the user asks to **识 bug / 视觉 review / review-prep / 打开截图修 bug / closed-loop visual fix**, or when a showcase BMP/PNG under `out/*/captures/` needs Agent inspection before code changes.

## Hard rules

1. **No fix before human confirm** — unless the current user message already says to fix a listed set of bugs.
2. Prefer **`*.inspect.png`** for `Read` (BMP often fails vision). Create via `--review-prep` if missing.
3. Split **crash / heap / AV** verify from **visual** fixes (separate agent or phase).
4. Stay on **`master`**. Prefer `build.bat debug <single_target>`.
5. Do **not** treat review as default `build.bat te`.

## Workflow

```bat
py -3 testing/tools/loop_runner.py --suite <id> --review-prep
rem re-exec showcase instead of reusing BMP:
py -3 testing/tools/loop_runner.py --suite <id> --review-prep --force-run --no-build
```

Artifacts under `out/<config>/captures/<scenario>/` (family dirs: `atmosphere/` `map2d/` `plugin/` `ui/` `legacy/` `shell/`; plus `record/` `analysis/` `_scratch/`):

| File | Role |
| --- | --- |
| `*.bmp` | Showcase capture |
| `*.inspect.png` | Agent-readable PNG |
| `*_visual_review.json` | `status=pending` stub + score snapshot + checklist |

### Agent steps

1. Run `--review-prep` (or open existing inspect PNG).
2. `Read` the inspect PNG (+ marks / score in review JSON).
3. Emit a **numbered bug table** (severity; product bug vs harness gate gap).
4. **Stop and wait** for human confirm / selection.
5. Fix confirmed items → rebuild → re-run suite → re-`Read` + `score_bmp`.
6. If a visual miss was not caught by `score_id`, **tighten the gate** in `testing/tools/loop/score/bmp.py` + suite `score_id`.
7. Update review JSON `status` to `verified` when done (`confirmed` / `fixing` optional intermediate).

### Status values

`pending` → `confirmed` → `fixing` → `verified`

Runner only writes `pending`. Agent/human may rewrite the JSON.

## Related

- Living: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Visual review closed-loop
- Plan: `docs/superpowers/plans/2026-10-01-harness-visual-review.md`
- As-built: `docs/build/ui-testing.md` §Visual review closed-loop
- Code: `testing/tools/loop/review/`
