# Task 6 Report: Print preview (`smartgis.print`)

**Status:** DONE
**Commit:** none (per task brief)

## Summary

Added `PrintPreviewDialog` (shared `MapPreviewView` + Save button using `ui::views::pick_save_file`) and `register_print`, which contributes command/dialog `print.preview` with an `open_dialog` handler. Leftover `CDlg2DXView` / MFC DLL untouched. No second preview type.

## Compile

```
.\build\bin\gn.exe gen out --root=./ --args="is_debug=true is_build_third_party=false"
.\build\bin\ninja.exe -C out src/plugin/print:print_views
```

Exit code: **0** (2 CXX objects: `print_preview_dialog.obj`, `print_commands.obj`).

## Implementation

- `PrintPreviewDialog` — Views shell with `MapPreviewView` child and Save button; Save invokes `pick_save_file` with image filter (no render-device save yet).
- `register_print` — `contribute_dialog` + `contribute_command` for `print.preview`; command handler calls `host->open_dialog("print.preview")`.
- `source_set("print_views")` added beside existing `plugin_print` MFC DLL in `src/plugin/print/BUILD.gn`.

## Graph wiring

`//src/plugin/print:print_views` was not reachable until `src/BUILD.gn` `src_all` gained that dep (foundation had host/widgets only). Required for `ninja -C out src/plugin/print:print_views`.

## Files

| File | Action |
| --- | --- |
| `src/plugin/print/print_preview_dialog.h` | Created |
| `src/plugin/print/print_preview_dialog.cc` | Created |
| `src/plugin/print/print_commands.h` | Created |
| `src/plugin/print/print_commands.cc` | Created |
| `src/plugin/print/BUILD.gn` | Modified — added `print_views` |
| `src/BUILD.gn` | Modified — `src_all` dep on `print_views` (GN reachability) |

## Not touched

`host_test.cc`, `src/plugin/BUILD.gn`, `dlg_2d_xview.*`, other domains.
