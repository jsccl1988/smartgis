# Task C Report: gis panels + map hang

**Status:** DONE_WITH_CONCERNS  
**Commit:** none (per brief)

## Summary

Moved seven GIS panel public headers from `src/ui/views/` into `src/ui/views/gis/` via `git mv`. Moved `map_viewport` / `touch_multitouch` headers and implementations into `src/ui/views/map/` (`git mv` for tracked paths; filesystem move for previously untracked touch files). Updated `#include` paths and include guards inside `gis/` and `map/` only, aligned with Task A kernel/primitives paths and the plan mapping table.

## Moves

### Panel headers → `gis/` (git mv)

| From | To |
| --- | --- |
| `catalog_view.h` | `gis/catalog_view.h` |
| `layer_tree.h` | `gis/layer_tree.h` |
| `attribute_table.h` | `gis/attribute_table.h` |
| `feature_info.h` | `gis/feature_info.h` |
| `status_bar.h` | `gis/status_bar.h` |
| `ambox_view.h` | `gis/ambox_view.h` |
| `chart_view.h` | `gis/chart_view.h` |

Panel `.cc` files were already under `gis/`; includes updated in place (seven panel impls only).

### Map hang → `map/`

| From | To | Method |
| --- | --- | --- |
| `map_viewport.h` | `map/map_viewport.h` | git mv |
| `gis/map_viewport.cc` | `map/map_viewport.cc` | git mv |
| `touch_multitouch.h` | `map/touch_multitouch.h` | Move-Item (untracked) |
| `gis/touch_multitouch.cc` | `map/touch_multitouch.cc` | Move-Item (untracked) |

## Include / guard updates (in-scope only)

- GIS panel headers: `UI_VIEWS_GIS_*` (e.g. `UI_VIEWS_GIS_CATALOG_VIEW_H_`).
- Map hang headers: `UI_VIEWS_MAP_VIEWPORT_H_`, `UI_VIEWS_MAP_TOUCH_MULTITOUCH_H_`.
- Kernel/primitives includes in panel `.cc` and headers → `ui/views/kernel/…`, `ui/views/primitives/…`.
- GIS sibling includes → `ui/views/gis/…`.
- Map includes → `ui/views/map/map_viewport.h`, `ui/views/map/touch_multitouch.h`, `ui/views/kernel/view.h` (no `gis/*` panel headers).
- `chart_view.h`: optional dialog include → `ui/views/dialogs/dialog.h` (`__has_include` path updated).

## Re-verification (2026-09-19)

Against `task-C-brief.md`; **no file moves or include edits required** on this pass.

| Check | Result |
| --- | --- |
| Seven panel `.h` + `.cc` under `gis/` | All present |
| Root `catalog_view.h` … `chart_view.h`, `map_viewport.h`, `touch_multitouch.h` | Absent |
| `gis/map_viewport.cc`, `gis/touch_multitouch.cc` | Absent |
| `map/` has `map_viewport.*`, `touch_multitouch.*` | Present (4 files) |
| `map/` `#include "ui/views/gis/…"` | None |
| Panel + map in-scope old flat `#include "ui/views/<stem>.h"` (kernel/primitives/gis/map stems) | None (`rg` on 14 panel files + `map/`) |
| Guards on panel headers | `UI_VIEWS_GIS_*` |
| Guards on map headers | `UI_VIEWS_MAP_*` |

## Explicitly not touched (per brief)

- `src/ui/views/BUILD.gn`, `views.h`, `views.cc`
- `src/app/**`, `docs/**`, `testing/`, `dialogs/`
- Dialog `.cc` under `gis/` (Task B); not edited even if still on disk or shown as deleted in git index

## Concerns / follow-ups

1. **`touch_multitouch.*` untracked:** Not in git index before move; still `??` under `map/`. Track D should add to GN sources when wiring paths.
2. **Build break until Track D:** GN, umbrella headers, and out-of-tree callers still use old flat includes; compile expected to fail until D updates `BUILD.gn` / `views.h` / `src/app`.
3. **`chart_view.h` vs Task B:** Optional include targets `ui/views/dialogs/dialog.h`; until B lands dialog headers under `dialogs/`, `UI_VIEWS_CHART_HAS_DIALOG` stays off.
4. **Parallel B churn:** Git index shows dialog `.cc` deletions from `gis/`; panel work did not modify dialog sources.

## Verification not run

No `build.bat` / compile (BUILD.gn intentionally stale for this track).
