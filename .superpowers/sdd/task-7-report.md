# Task 7 Report: Map service Views (`smartgis.map_service`)

**Status:** DONE

## Summary

Added Views rewrite for map service plugin-host: `ServiceMgrDialog` (service `TableView`, config `TabStrip`, lifecycle buttons), `SvrCfgBasePage` / `SvrCfgTilePage` (leftover fields + `MapPreviewView` for Review Map / View Tiles), `MapClientDialog` (client canvas is `MapPreviewView`), and `register_map_service` wiring commands/dialogs. Legacy `SmtMapService` integration is stubbed in `map_service_commands.cc` (service list empty; create/generate-tiles show info); lifecycle handlers use `ShellExecuteW` on `SmtWSMapSvr.exe` like leftover MFC.

## Compile

```
.\build\bin\gn.exe gen out --root=./
.\build\bin\ninja.exe -C out src/plugin/map_service:map_service_views
Exit code: 0
```

Note: `//src/plugin/map_service:map_service_views` was wired into `//src:src_all` (same pattern as `print_views`) so GN loads `src/plugin/map_service/BUILD.gn`.

## APIs

- `plugin::register_map_service(content::PluginHost*)` — contributes `map_service.manage` → dialog `map_service.mgr`, `map_service.client` → `MapClientDialog`, plus `map_service.install|start|stop|uninstall|restart`.
- `plugin::map_service_names()` — stub returning `{}` until `SmtMapServiceMgr` is linked without MFC.

## Files

| File | Action |
| --- | --- |
| `src/plugin/map_service/map_service_commands.h` | Created |
| `src/plugin/map_service/map_service_commands.cc` | Created |
| `src/plugin/map_service/service_mgr_dialog.h` | Created |
| `src/plugin/map_service/service_mgr_dialog.cc` | Created |
| `src/plugin/map_service/svr_cfg_base_page.h` | Created |
| `src/plugin/map_service/svr_cfg_base_page.cc` | Created |
| `src/plugin/map_service/svr_cfg_tile_page.h` | Created |
| `src/plugin/map_service/svr_cfg_tile_page.cc` | Created |
| `src/plugin/map_service/map_client_dialog.h` | Created |
| `src/plugin/map_service/map_client_dialog.cc` | Created |
| `src/plugin/map_service/BUILD.gn` | Modified — added `source_set("map_service_views")` |
| `src/BUILD.gn` | Modified — added `map_service_views` to `src_all` (GN graph) |

## Not touched

`host_test.cc`, `src/plugin/BUILD.gn`, leftover `dlg_*` sources.

No commit (per brief).
