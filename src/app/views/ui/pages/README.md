<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `ui/pages` — MapPagesComposer

Map / Data / 3D tab horizon for `BrowserView`. Public type is **`app::MapPagesComposer`**
(`map_pages_composer.h`). Implementation is **same-class multi-TU** by responsibility:

| TU | Responsibility |
| --- | --- |
| `map_pages_composer.cc` | ctor, flash timer, `for_each_draw_host`, active map/host/size |
| `map_pages_viewport.cc` | `attach_viewports` |
| `map_pages_scene_wire.cc` | `wire_map_scene` (2D/3D overlay paint + `present_gpu`) |
| `map_pages_shell_overlay.cc` | shell DIB crop commit + overlay invalidate |
| `map_pages_tool_seams.cc` | Workspace draft / hit / nav / project binds |
| `map_pages_gestures.cc` | HWND pinch/pan + right-click |
| `map_pages_tab_switch.cc` | tab switch, lazy 3D attach, China atmo/orbit |
| `detail/ptr_guard.h` | poison / readable pointer checks |
| `detail/seh_workspace.*` | SEH wrappers around ToolSession / Workspace |

Living shell: **§shell/ui composers** in
[`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md).
