<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task C brief — gis panels + map hang

**Plan:** `docs/superpowers/plans/2026-09-19-ui-views-subdir-responsibility.md`  
**Do NOT commit. Do NOT edit BUILD.gn / views.h / views.cc / src/app / docs / testing / dialogs/.**

## Goal

1. Move GIS **panel** headers from root into `gis/`  
2. Move MapViewport + TouchMultitouch (headers + `.cc`) into `map/`  
3. Update includes **only** inside `gis/` and `map/`

## git mv panel headers → gis/

catalog_view.h, layer_tree.h, attribute_table.h, feature_info.h, status_bar.h, ambox_view.h, chart_view.h

## git mv map hang → map/

From root: map_viewport.h, touch_multitouch.h  
From gis/: map_viewport.cc, touch_multitouch.cc

**Do not move** create_* / att_struct / add_basemap dialog `.cc` if still under gis/ — Task B owns those. If they are still there when you start, leave them for B (do not delete).

## Includes

- Update `gis/` + `map/` internals to new kernel/primitives/gis/map paths (Task A already relocated kernel/primitives headers).
- `map/` must **not** include `gis/*` panel headers.
- Guards: `UI_VIEWS_GIS_*` / `UI_VIEWS_MAP_*`

## Parallel note

Task B may be moving dialog files out of `gis/` concurrently. Only touch panel `.cc` already in gis/ and the map hang files listed above. Avoid editing the same dialog files B is moving.

## Report

`.superpowers/sdd/briefs/task-C-report.md`
