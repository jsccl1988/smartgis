<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/sys`

Leftover `SmtSysManager` (style / map-doc / project / sys parameters).

| Item | Notes |
| --- | --- |
| Sources | `sysmanager.cpp` → `sys_sources` |
| DLL | Absorbed into **`base.dll`** (`//src/base:base`), not a separate sys DLL |
| Include | `#include "legacy/sys/sysmanager.h"` |
| GN alias | `//src/legacy/sys:sys` → `//src/base:base` |

Views / `src/app` / `src/content` do **not** use this type. Delete when leftover MFC callers are gone.
