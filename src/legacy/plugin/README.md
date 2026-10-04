<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/plugin`

Leftover AuxModule runtime and domain MFC plugin DLLs (`*.am` / LoadLibrary). Not the Views endgame — builtins live under `src/plugin/product/*` with host under `src/plugin/runtime/host`. Stay under `legacy/`; do not move these sources into `src/plugin/`.

## Layout

Top concepts: **`runtime/`** (leftover AuxModule + host bridge) and **`product/<domain>/`** (MFC shells).

```
legacy/plugin/
  BUILD.gn  README.md
  runtime/
    auxmodule/      # SmtAuxModule ABI → //src/legacy/plugin:plugin
    bridge/         # *.am scan (am.cc) + header-only AM_MSG map (cmd.h)
  product/
    <domain>/
      BUILD.gn
      shell/
      views/
      res/
      kernel/       # orthogrid only
```

| Path | Role | GN |
| --- | --- | --- |
| `runtime/auxmodule/` | AuxModule manager / msg / MFC helpers | `//src/legacy/plugin:plugin` |
| `runtime/bridge/` | `*.am` → Registry + `AM_MSG_*` → catalog (`cmd.h` header-only) | `//src/legacy/plugin/runtime:bridge` |
| `product/<domain>/shell/` | DLL entry / plug / creater / pch / rc / def | `plugin_dem` / … |
| `product/<domain>/views/` | MFC `dlg_*` (omit if none) | same DLL |
| `product/orthogrid/kernel/` | 2010 `Orthogrid` class | `:orthogrid_kernel` |

Includes use scheme C (`legacy/plugin/runtime/auxmodule/…`, `legacy/plugin/runtime/bridge/…`); no shim. Do not use directory name `aux/` (Windows device name).

## Docs

- Plan: [`docs/superpowers/plans/2026-09-29-legacy-plugin-subdirectory-layout.md`](../../../docs/superpowers/plans/2026-09-29-legacy-plugin-subdirectory-layout.md)
- Living: [`docs/superpowers/specs/2026-09-13-plugin-host-design.md`](../../../docs/superpowers/specs/2026-09-13-plugin-host-design.md)
- As-built: [`docs/superpowers/src-layout.md`](../../../docs/superpowers/src-layout.md)

Preserve `Smt_*` / DEF ABI.
