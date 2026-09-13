# Task 5 Report: Projection Views (`smartgis.proj`)

**Status:** DONE

## Summary

Added Views-based projection transform UI for `smartgis.proj`: `MapPrjDialog` with `TabStrip` hosting `MapPrjGridPage` and `MapPrjXyPage`, plus `register_proj` wiring command `proj.do_prj`, dialog `proj.dialog`, and processing ids `proj.transform_xy` / `proj.transform_grid`.

## Compile

```
build.bat src/plugin/proj:proj_views
Exit code: 0
```

All four `proj_views` objects built (`map_prj_dialog`, `map_prj_grid_page`, `map_prj_xy_page`, `proj_commands`).

## Implementation

| Component | Role |
| --- | --- |
| `MapPrjDialog` | `TabStrip` with Grid + XY pages; syncs scale ruler from grid to XY |
| `MapPrjXyPage` | L, B, X, Y fields + scale label; Apply → `run_processing("proj.transform_xy", json)` |
| `MapPrjGridPage` | dL, dB, Lmin/Bmin/Lmax/Bmax, scale; Apply → `run_processing("proj.transform_grid", json)` |
| `register_proj` | Contributes `proj.do_prj` (投影变换) → `open_dialog("proj.dialog")`; registers dialog + processing |
| `proj_commands.cc` | Processing factories call `Smt_Prj` projection API (`PLUGIN_PROJ_VIEWS_USE_PROJ_API`); XY results via `consume_transform_xy_output()` |

Default field values match legacy `CDlgMapPrjDoXY` / `CDlgMapPrjDoGrid`. No PROJ headers in dialog `.cc` files.

## GN

Added `source_set("proj_views")` to `src/plugin/proj/BUILD.gn` (leftover `plugin_proj` / `SmtAMMapProject` unchanged). Target is reachable via existing `plugin_host_test` deps in `src/plugin/BUILD.gn`.

## Files

| File | Action |
| --- | --- |
| `src/plugin/proj/map_prj_dialog.h` | Created |
| `src/plugin/proj/map_prj_dialog.cc` | Created |
| `src/plugin/proj/map_prj_xy_page.h` | Created |
| `src/plugin/proj/map_prj_xy_page.cc` | Created |
| `src/plugin/proj/map_prj_grid_page.h` | Created |
| `src/plugin/proj/map_prj_grid_page.cc` | Created |
| `src/plugin/proj/proj_commands.h` | Created |
| `src/plugin/proj/proj_commands.cc` | Created |
| `src/plugin/proj/BUILD.gn` | Modified — added `proj_views` source_set |

## Not touched

`host_test.cc`, `src/plugin/BUILD.gn`, other domain trees, `src/content/**`, `src/ui/views/**`, `src/plugin/widgets/**`.

## Notes

- Grid processing validates and projects the grid nodes but does not append to the map layer (legacy MFC path); map integration is deferred.
- Initial build hit a transient `Permission denied` on `winservice.obj` from parallel ninja processes; resolved by killing stale ninja/cl and rebuilding.
