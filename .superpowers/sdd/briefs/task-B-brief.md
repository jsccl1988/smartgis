<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task B brief — dialogs

**Plan:** `docs/superpowers/plans/2026-09-19-ui-views-subdir-responsibility.md`  
**Do NOT commit. Do NOT edit BUILD.gn / views.h / views.cc / src/app / docs / testing.**

## Goal

Move **all** dialogs (generic + GIS) into `src/ui/views/dialogs/` (headers + `.cc`). Update includes **only** inside `dialogs/`.

## git mv headers (from root)

dialog.h, file_picker.h, message_box.h, input_text_dialog.h, select_one_dialog.h, create_datasource_dialog.h, create_layer_dialog.h, create_map_dialog.h, att_struct_dialog.h, add_basemap_dialog.h → `dialogs/`

## git mv sources

From `primitives/`: dialog.cc, file_picker.cc, message_box.cc, input_text_dialog.cc, select_one_dialog.cc → `dialogs/`  
From `gis/`: create_datasource_dialog.cc, create_layer_dialog.cc, create_map_dialog.cc, att_struct_dialog.cc, add_basemap_dialog.cc → `dialogs/`

## Includes

Inside `dialogs/` only: update to `ui/views/kernel/…`, `ui/views/primitives/…`, `ui/views/dialogs/…` as needed. Guards → `UI_VIEWS_DIALOGS_*`.  
Note: Task A already moved kernel/primitives headers; use those new paths when including them from dialogs.

## Do not touch

gis panel sources (catalog etc.), map/, BUILD.gn, app, testing.

## Report

`.superpowers/sdd/briefs/task-B-report.md`
