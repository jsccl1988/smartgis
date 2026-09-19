<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task B Report: dialogs co-location

**Status:** DONE_WITH_CONCERNS  
**Commit:** none (per brief)

## Summary

Moved 10 dialog headers from `src/ui/views/` and 10 dialog `.cc` files from `primitives/` (5) and `gis/` (5) into `src/ui/views/dialogs/` via `git mv`. Updated `#include` paths and include guards **only** inside `dialogs/`.

## Moves (git mv)

### Headers → `dialogs/`

dialog.h, file_picker.h, message_box.h, input_text_dialog.h, select_one_dialog.h, create_datasource_dialog.h, create_layer_dialog.h, create_map_dialog.h, att_struct_dialog.h, add_basemap_dialog.h

### Sources → `dialogs/`

| From | Files |
| --- | --- |
| `primitives/` | dialog.cc, file_picker.cc, message_box.cc, input_text_dialog.cc, select_one_dialog.cc |
| `gis/` | create_datasource_dialog.cc, create_layer_dialog.cc, create_map_dialog.cc, att_struct_dialog.cc, add_basemap_dialog.cc |

## Include / guard updates (in-scope only)

- Dialog headers and peers: `"ui/views/dialogs/<stem>.h"`.
- Kernel / primitives deps per mapping table: `"ui/views/kernel/…"`, `"ui/views/primitives/…"`.
- Include guards: `UI_VIEWS_DIALOGS_*` (e.g. `UI_VIEWS_DIALOGS_DIALOG_H_`).
- Namespace unchanged: `ui::views` only.

## Self-check

- `primitives/` and `gis/`: **no** dialog `.cc` / dialog-only headers remain.
- `src/ui/views/` root: **no** dialog headers remain (only `kernel/dialog_host.h` for modal chrome).
- Grep under `dialogs/` for flat `ui/views/<stem>.h` (unqualified kernel/primitives/dialog stems): **no matches**.

## Files touched (this task)

| Category | Count |
| --- | ---: |
| Headers moved + guard edits | 10 |
| Sources moved + include edits | 10 |
| **Total under `dialogs/`** | **20** |

## Explicitly not touched (per brief)

- `src/ui/views/BUILD.gn`
- `src/ui/views/views.h`, `views.cc`
- `src/app/**`
- `docs/**`, `testing/**`
- `gis/` panel sources (catalog, layer_tree, etc.), `map/`
- No root shim headers for moved dialog files

## Concerns / follow-ups for later tracks

1. **Build break until Track D:** GN `sources` still list old paths (`primitives/dialog.cc`, root `dialog.h`, etc.). `views.h` and callers outside `dialogs/` still use `"ui/views/dialog.h"` and similar until D updates them.
2. **Task A leftovers:** `kernel/` and `primitives/` `.cc` may still `#include` flat dialog paths from before B; only `dialogs/` was updated in this task.
3. **Parallel Task C:** GIS/map header moves may churn adjacent paths; merge with D’s BUILD.gn pass carefully.

## Verification not run

Compile/e2e not run (BUILD.gn intentionally stale). Recommend Track D apply GN + umbrella includes then `build.bat` smoke.
