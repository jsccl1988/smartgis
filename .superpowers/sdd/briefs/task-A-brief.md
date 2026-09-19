<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task A brief — kernel + primitives

**Plan:** `docs/superpowers/plans/2026-09-19-ui-views-subdir-responsibility.md`  
**Spec:** `docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md`  
**Work dir:** `C:/Dev/src/gis/smartgis`  
**Do NOT commit** (unless user later asks).

## Goal

Move kernel + primitives **headers** next to their `.cc` files. Update includes **only** inside `kernel/` and `primitives/` files. Leave dialogs `.cc` in `primitives/` for Task B. Do **not** edit `BUILD.gn`, `views.h`, `views.cc`, `src/app/**`, or docs.

## Steps (verbatim from plan)

1. `git mv` kernel headers into `kernel/`:
   - view.h, widget.h, layout.h, layout_check.h, theme.h, event.h, dpi.h, splitter.h, dialog_host.h

2. `git mv` primitives headers (**exclude dialogs**) into `primitives/`:
   - button, label, textfield, checkbox, radio_button, combobox, tab_strip, table_view, tree_view, scroll_view, menu_bar, context_menu

3. Update `#include "ui/views/<stem>.h"` inside `kernel/` + `primitives/` only → new paths per mapping. Include guards → `UI_VIEWS_KERNEL_*` / `UI_VIEWS_PRIMITIVES_*`.

4. Self-check: no old paths left inside those two dirs for moved stems.

5. Do not touch BUILD.gn / views.h / app.

## Include mapping (this task)

| Old | New |
| --- | --- |
| ui/views/view.h | ui/views/kernel/view.h |
| ui/views/widget.h | ui/views/kernel/widget.h |
| ui/views/layout.h | ui/views/kernel/layout.h |
| ui/views/layout_check.h | ui/views/kernel/layout_check.h |
| ui/views/theme.h | ui/views/kernel/theme.h |
| ui/views/event.h | ui/views/kernel/event.h |
| ui/views/dpi.h | ui/views/kernel/dpi.h |
| ui/views/splitter.h | ui/views/kernel/splitter.h |
| ui/views/dialog_host.h | ui/views/kernel/dialog_host.h |
| ui/views/button.h | ui/views/primitives/button.h |
| ui/views/label.h | ui/views/primitives/label.h |
| ui/views/textfield.h | ui/views/primitives/textfield.h |
| ui/views/checkbox.h | ui/views/primitives/checkbox.h |
| ui/views/radio_button.h | ui/views/primitives/radio_button.h |
| ui/views/combobox.h | ui/views/primitives/combobox.h |
| ui/views/tab_strip.h | ui/views/primitives/tab_strip.h |
| ui/views/table_view.h | ui/views/primitives/table_view.h |
| ui/views/tree_view.h | ui/views/primitives/tree_view.h |
| ui/views/scroll_view.h | ui/views/primitives/scroll_view.h |
| ui/views/menu_bar.h | ui/views/primitives/menu_bar.h |
| ui/views/context_menu.h | ui/views/primitives/context_menu.h |

## Global constraints

- Namespace: only `ui::views` (no `ui::views::kernel`)
- No root shim headers for moved files
- Single GN target (D owns BUILD.gn)
- Master branch only
- Comments in English if you touch them

## Report

Write full report to: `.superpowers/sdd/briefs/task-A-report.md`  
Return status only (DONE / DONE_WITH_CONCERNS / BLOCKED / NEEDS_CONTEXT), files changed count, concerns, report path.
