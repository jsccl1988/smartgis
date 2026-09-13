# Task 8 Report: model3d commands (`smartgis.model3d`)

**Status:** DONE

## Summary

Added `plugin::register_model3d(content::PluginHost*)` with nine command ids mapped from leftover `SMT_MSG_3DMODELCREATER_1`–`9`. Handlers use `ui::views::pick_open_file` and `ui::views::show_message_box` only; no new `CDialog`. Legacy scene/map types are not linked into `model3d_views` — scene hooks in `detail` return null/false so `execute("model3d.add_sphere")` returns false without crashing when no 3D scene is attached.

## Commands

| id | Leftover msg | Handler behavior (no scene) |
| --- | --- | --- |
| `model3d.add_pointcloud` | `_1` | File picker; error message if cancelled; false if no scene |
| `model3d.add_sphere` | `_2` | false |
| `model3d.add_water` | `_3` | false |
| `model3d.add_terrain_grid` | `_4` | false |
| `model3d.add_terrain_tin` | `_5` | false |
| `model3d.create_tin` | `_6` | false |
| `model3d.layer_points_to_3d` | `_7` | false (silent); message if scene present but layer wrong |
| `model3d.layer_lines_to_3d` | `_8` | same pattern |
| `model3d.layer_polygons_to_3d` | `_9` | same pattern |

Plugin id: `smartgis.model3d`. Menu parent: `tools`.

## GN

Added `source_set("model3d_views")` to `src/plugin/model3d/BUILD.gn` (leftover `plugin_model3d` DLL unchanged). Target is wired in `src/BUILD.gn` `src_all` (already present when compile ran).

## Compile

```
.\build\bin\gn.exe gen out --root=./ --args="is_debug=true is_build_third_party=false"
.\build\bin\ninja.exe -C out src/plugin/model3d:model3d_views
```

**Result:** exit code 0 (green).

## Files

| Path | Change |
| --- | --- |
| `src/plugin/model3d/model3d_commands.h` | Created |
| `src/plugin/model3d/model3d_commands.cc` | Created |
| `src/plugin/model3d/BUILD.gn` | Added `model3d_views` source_set |

## Notes

- Command titles and message-box strings use ASCII English for MSVC MBCS build (repo does not use `/utf-8`).
- Scene mutation stubs in `detail` are the extension point for wiring `SmtSceneMgr` when legacy types can link into this source_set without MFC.
- `host_test.cc` and `src/plugin/BUILD.gn` not modified per brief.
