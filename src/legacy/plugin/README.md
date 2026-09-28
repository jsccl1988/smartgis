<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/plugin`

Leftover AuxModule runtime and domain MFC plugin DLLs (`*.am` / LoadLibrary). Not the Views endgame — builtins live under `src/plugin/product/*` with host under `src/plugin/runtime/host`.

## Layout

| Path | Role | GN |
| --- | --- | --- |
| `module*`, `plugin_msg*` | AuxModule manager + message table | `//src/legacy/plugin:plugin` (`dll_stem=plugin`) |
| `adapter/` | `*.am` scan + `AM_MSG_*` → catalog (`source_set`) | `:am`, `:cmd` (`:legacy_*` aliases) |
| `dem/` `proj/` `print/` `model3d/` `orthogrid/` | Domain MFC shells | `plugin_dem` / `plugin_proj` / … |
| `orthogrid/kernel/` | 2010 `Orthogrid` class | `:orthogrid_kernel` |

Includes use `legacy/plugin/….h` (no extra `legacy_` file prefix under this tree). Root `//src/plugin:plugin` and `//src/plugin:cmd` re-export leftover labels (`:legacy_cmd` alias kept).

## Docs

- Freeze (landed): [`docs/superpowers/archive/specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](../../../docs/superpowers/archive/specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md)
- Parent L1: [`docs/superpowers/specs/2026-09-14-plugin-subdir-layout-design.md`](../../../docs/superpowers/specs/2026-09-14-plugin-subdir-layout-design.md)
- As-built table: [`docs/build/src-layout.md`](../../../docs/build/src-layout.md)

Do not nest AuxModule sources into a deeper bucket or reshuffle `dlg_*` without revising the freeze spec. Preserve `Smt_*` / DEF ABI.
