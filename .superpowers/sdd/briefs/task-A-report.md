# Task A Report: kernel + primitives header co-location

**Status:** DONE_WITH_CONCERNS  
**Commit:** none (per brief)

## Summary

Moved 9 kernel and 12 primitives public headers from `src/ui/views/` into `src/ui/views/kernel/` and `src/ui/views/primitives/` via `git mv`. Updated `#include` paths and include guards inside those two directories only. Dialog `.cc` files remain under `primitives/`; dialog headers stay at repo root for Task B.

## Moves (git mv)

### Kernel → `kernel/`

| Header |
| --- |
| view.h, widget.h, layout.h, layout_check.h, theme.h, event.h, dpi.h, splitter.h, dialog_host.h |

### Primitives → `primitives/` (dialogs excluded)

| Header |
| --- |
| button.h, label.h, textfield.h, checkbox.h, radio_button.h, combobox.h, tab_strip.h, table_view.h, tree_view.h, scroll_view.h, menu_bar.h, context_menu.h |

## Include / guard updates (in-scope only)

- All `#include "ui/views/<stem>.h"` for moved stems under `kernel/` and `primitives/` now use the mapping table in the task brief (e.g. `ui/views/kernel/view.h`, `ui/views/primitives/button.h`).
- Unmoved stems referenced from those dirs unchanged (e.g. `ui/views/dialog.h`, `ui/views/select_one_dialog.h`, `ui/views/file_picker.h`, `ui/views/message_box.h`).
- Include guards on moved headers:
  - Kernel: `UI_VIEWS_KERNEL_*` (e.g. `UI_VIEWS_KERNEL_VIEW_H_`).
  - Primitives: `UI_VIEWS_PRIMITIVES_*` (e.g. `UI_VIEWS_PRIMITIVES_BUTTON_H_`).
- Namespace unchanged: `ui::views` only.

## Self-check

- Grep under `kernel/` and `primitives/` for old flat includes of moved stems: **no matches**.
- Old guard names (`UI_VIEWS_VIEW_H_`, `UI_VIEWS_BUTTON_H_`, etc.) on moved headers in those dirs: **no matches**.

## Files touched (this task)

| Category | Count |
| --- | ---: |
| Headers renamed + guard/include edits | 21 |
| `.cc` under `kernel/` | 8 |
| `.cc` under `primitives/` (incl. dialog impls) | 17 |
| **Total paths with content/guard changes** | **46** |

## Explicitly not touched (per brief)

- `src/ui/views/BUILD.gn`
- `src/ui/views/views.h`, `views.cc`
- `src/app/**`
- `docs/**`
- `dialogs/`, `gis/`, `map/` trees (except pre-existing unrelated workspace churn elsewhere)
- No root shim headers for moved files

## Concerns / follow-ups for later tracks

1. **Build break until Track D:** GN `sources` and `views.h` still reference `ui/views/<stem>.h` at the old paths. Expect compile failures for any target including those headers until BUILD.gn and umbrella includes are updated.
2. **Out-of-tree includes:** Root-level GIS/dialog headers (`chart_view.h`, `map_viewport.h`, etc.), tests (`views_unittests.cc`, pixel tests), and `src/app/**` still include old paths; Tracks B–D own those updates.
3. **Guard macro rename:** External code that tested old guard names (unlikely) would need updating when callers switch to new header paths.

## Verification not run

Compile/e2e not run in this task (BUILD.gn intentionally stale). Recommend Track D apply GN + `views.h` then `build.bat` smoke.
