<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task D Report: testing + BUILD.gn + views.h + callers + docs

**Status:** DONE_WITH_CONCERNS  
**Commit:** none (per brief)

## Summary

Moved pixel harness / unit+pixel tests / goldens into `src/ui/views/testing/`, rewrote `BUILD.gn` and umbrella `views.h` for the six responsibility partitions, replaced flat `#include "ui/views/<stem>.h"` across `src/` (app + plugins + testing), deleted root forwarding shims, and updated nesting docs. `build.bat views` succeeded; `views_unittests` and `views_pixel_tests` both ok.

## Moves (`git mv`)

| From | To |
| --- | --- |
| `pixel_harness.h` / `.cc` | `testing/` |
| `pixel_png_wic.cc` | `testing/` |
| `views_unittests.cc` | `testing/` |
| `views_pixel_tests.cc` | `testing/` |
| `testdata/` | `testing/testdata/` |

Golden path helper now resolves `src/ui/views/testing/testdata` (via `SMARTGIS_ROOT` or cwd-relative).

## GN / umbrella

- `source_set("views")` sources list headers+`.cc` under `kernel/` `primitives/` `dialogs/` `gis/` `map/` plus root `views.h` / `views.cc`.
- `views_unittests` / `views_pixel_tests` sources point at `testing/…`.
- `views.h` enumerates the full ownership table (including `dialog_host` and `touch_multitouch`).

## Callers

Repo-wide replace of flat includes (longest-key-first to avoid `dialog.h` clobbering `dialog_host.h`). Touched e.g.:

- `src/app/views/**`
- `src/plugin/**` (host, widgets, dem, print, proj, …)
- `src/ui/views/testing/**`

Acceptance: no `#include "ui/views/<flat_stem>.h"` under `src` except via umbrella semantics; root business headers removed.

## Docs

| File | Change |
| --- | --- |
| `src/ui/views/README.md` | Six-partition layout; include prefixes |
| `docs/build/ui-views-skia.md` | Nesting cap allows responsibility partitions |
| `docs/build/ui-testing.md` | Paths under `testing/`; `kernel/layout_check.h` |
| `docs/build/src-layout.md` | Example `"ui/views/kernel/view.h"` |
| `docs/superpowers/specs/2026-09-13-ui-views-controls-design.md` | Status note: nesting superseded by 2026-09-19 design |
| `docs/superpowers/plans/2026-09-19-ui-views-subdir-responsibility.md` | Restored after encoding mishap (see concerns) |

## Root cleanup

Deleted leftover `UI_VIEWS_*_SHIM_H_` forwarding headers at `src/ui/views/*.h` (all stems except `views.h`). Final check: only `views.h` among root headers.

## Verify

```bat
build.bat views          → exit 0 (SmartGisViews.exe linked)
out\views_unittests.exe  → views_unittests: ok
out\views_pixel_tests.exe → views_pixel_tests: ok
```

## Concerns

1. **Root shims resurrected mid-flight** — while D ran, forwarding headers at `src/ui/views/*.h` reappeared (likely parallel A/B/C or review tooling). Deleted again; coordinator should confirm no agent re-adds shims after D.
2. **Encoding trap** — naive PowerShell `Get-Content`/`Set-Content` on UTF-8 Chinese docs briefly corrupted the 2026-09-19 plan; restored by rewrite. Prefer UTF-8-aware writes for future bulk doc edits; avoid running the include map over `docs/superpowers/plans/*` identity examples.
3. **Historical docs** outside the owned list (older plugin plans/specs) may still mention flat `ui/views/foo.h` in prose; not rewritten this pass except where the bulk replace already hit them before the docs restore.

## Owns (this task)

`BUILD.gn`, `views.h`, `views.cc`, `testing/**`, `src/app/views/**` includes, README + listed build/spec docs.
