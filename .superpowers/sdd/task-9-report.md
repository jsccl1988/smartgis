# Task 9 Report: baogrid commands (`smartgis.baogrid`)

**Status:** DONE_WITH_CONCERNS

## Summary

Implemented `plugin::register_baogrid` with four command handlers and `baogrid.create_orth_grid` processing stub. Boundary input commands delegate to `edit.append.linestring` when the workspace catalog contains that id; `save_boundary` returns false (leftover body was empty); `load_boundary` opens a file via `ui::views::pick_open_file` then calls `run_processing`. Processing factory stubs false (legacy `SmtBAOrthGrid` not wired into this source_set).

## Compile

```
.\build\bin\gn.exe gen out --root=./ --args="is_debug=true is_build_third_party=false"
.\build\bin\ninja.exe -C out src/plugin/baogrid:baogrid_views
Exit code: 0
```

## Concerns

- Added `//src/plugin/baogrid:baogrid_views` to `src/BUILD.gn` `src_all` so GN loads `src/plugin/baogrid/BUILD.gn` (same pattern as `print_views` / `model3d_views`). Target was unknown before that one-line dep.
- Command titles use English strings to avoid MSVC C4819/C2001 on CP936 when Chinese literals lack UTF-8 BOM.
- `baogrid.create_orth_grid` processing is a stub; grid kernel remains in legacy algorithm DLL.

## Files

| File | Action |
| --- | --- |
| `src/plugin/baogrid/baogrid_commands.h` | Created |
| `src/plugin/baogrid/baogrid_commands.cc` | Created |
| `src/plugin/baogrid/BUILD.gn` | Modified — added `source_set("baogrid_views")` |
| `src/BUILD.gn` | Modified — wired `baogrid_views` into `src_all` |

## API

```cpp
namespace plugin {
bool register_baogrid(content::PluginHost* host);
}
```

| Command id | Behavior |
| --- | --- |
| `baogrid.input_boundary_0` | `host->execute("edit.append.linestring", args)` if catalog attached; else false |
| `baogrid.input_boundary_2` | same with payload `boundary_2` |
| `baogrid.save_boundary` | returns false |
| `baogrid.load_boundary` | `pick_open_file` then `run_processing("baogrid.create_orth_grid", …)` |
