<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/core`

Leftover Smt core — **header-only** (STL internals). Still exposed through
`core_sources` → `//src/base:base` for include/deps hygiene; no `.cpp` in this
tree.

| Path | Role |
| --- | --- |
| `macros/macros.h` | Legacy Smt macros / `SmtErr` / TRACE; `dEPSILON` / `dPI` / `is_equal` |
| `types/` | `Point2`/`Point3`/`Rect` + traits (`normalize`/`contains`/`cast_to`); `SmtVariant`; `env.h` |
| `util/` | Inline helpers (`string` `path` `color` `image` `menu`) |
| `listener/listener_manager.h` | `SmtListener` + manager + post helpers |
| `command/` | Command stack |
| `msg/msg_def.h` | Message payloads / id ranges |
| `diag/` | Assert / exception |

Include: `#include "legacy/core/util/path.h"`. `util/image.h` pulls CxImage —
targets that include it must `deps += [ "//third_party:CxImage" ]`.
