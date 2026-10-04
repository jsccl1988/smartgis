<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/gis/datasource`

Leftover DSM catalog singleton (`DataSourceMgr`) compiled into `gis.dll` via
`sde_mgr_sources`. Mirrors product `gis/datasource` under the leftover tree.

- **Frozen:** bugfix only; do not grow new product paths here.
- **New code:** `//src/gis/datasource/session` (`DataSession` / `ConnectionSpec`).
- **Adapter:** `connection_spec_info.h` — leftover `SmtDataSourceInfo` ↔ product `ConnectionSpec`.
- Include: `#include "legacy/gis/datasource/datasource_mgr.h"`.
