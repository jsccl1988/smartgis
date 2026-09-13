<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin subdirectory layout (L1)

**Date:** 2026-09-14  
**Status:** accepted  
**Choices:** L1 + I1 + nested `legacy/<domain>` (not sibling `legacy_XXX`). No include shims.

## Target tree

```
src/plugin/
  BUILD.gn                 # group re-exports (:host :plugin :legacy_cmd)
  host/                    # Registry, store, processing, ManagerView, legacy_am/cmd, tests
  legacy/                  # leftover AuxModule + domain MFC shells
    module* / plugin_msg*
    dem/                   # dlg_*, creater, plug, stdafx, rc
    proj/
    print/
    model3d/
    orthogrid/
  widgets/
  python/
  samples/
  dem/                     # Views + MFC-free loaders only
  proj/ print/ model3d/ orthogrid/   # product Views (+ orthogrid kernels)
```

## Includes (I1)

| Old | New |
| --- | --- |
| `plugin/registry.h` | `plugin/host/registry.h` |
| `plugin/module.h` | `plugin/legacy/module.h` |
| `plugin/dem/dlg_*.h` | `plugin/legacy/dem/dlg_*.h` |
| domain Views | `plugin/<domain>/…` |

## GN

- `//src/plugin/host:host`
- `//src/plugin/legacy:plugin` (AuxModule DLL)
- `//src/plugin/legacy/dem:plugin_dem` (and proj/print/model3d/orthogrid)
- Root `//src/plugin:{host,plugin,legacy_cmd}` groups re-export

## Non-goals

- Do not rename stable plugin ids.
- Do not move widgets/python/samples.
