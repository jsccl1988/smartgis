<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `browser/plugin` — PluginShell role dirs

Chrome plugin platform for Views: Registry / CommandCatalog / ProcessingPool /
Python, PluginHost sink wiring, and plugin-agnostic present / ResultPlayback.
Product payload stays under `src/plugin/product`.

Parent layers (core vs China product vs harness):
[`../README.md`](../README.md).

Living shell: **§shell/ui composers** /
[`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md).
Plugin host model:
[`docs/superpowers/specs/2026-09-13-plugin-host-design.md`](../../../../../docs/superpowers/specs/2026-09-13-plugin-host-design.md).
Diagram:
[`docs/superpowers/diagrams/browser-plugin-subdirectory-layers.html`](../../../../../docs/superpowers/diagrams/browser-plugin-subdirectory-layers.html).

## Role inventory

| Dir | Role | Own here |
| --- | --- | --- |
| **`shell/`** | Platform ownership | `PluginShell`, `default_plugins_dir` / `peek_plugin_startup`, builtins append list, pack enable, `startup` apply |
| **`host/`** | PluginHost sinks | `host_context`, `plugin_host_wiring`, map2d / scene3d bridges (horizon-filled callbacks) |
| **`present/`** | Chrome present surface | tab/dataset present, `PluginPlayback`, shared `PluginPreviewHost` |
| **`harness/`** | Showcase / suite only | `report_suite` (FakeReportBrowser) |
| **`detail/`** | Shared helpers | `path_resolve` (exe-relative paths for bridges) |

```
PluginShell::init / attach_gis_contents
  → builtins + pack enable + Registry scan
plugin_host_wiring
  → install_map2d_host_bridges / install_scene3d_host_bridges
present_* / PluginPlayback / PluginPreviewHost
  → main tabs or surface=1 preview; ticks *.present_frame
report_suite
  → harness inspector path only
```

## Rules for edits

1. **New PluginHost capability** → narrow callback on `host/host_context.h`; wire
   product defaults once in `host/plugin_host_wiring.cc`.
2. **New present / playback chrome** → `present/` only; do not name flood /
   traffic / orthogrid stores.
3. **New pack / startup / builtins** → `shell/`; PluginShell must not `#include`
   product `commands.h` (packs append themselves).
4. **Harness report / fake browser** → `harness/` only.
5. **Do not** pull leftover / product present writers back into chrome.
