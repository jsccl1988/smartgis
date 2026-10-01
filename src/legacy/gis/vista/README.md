<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/gis/vista`

Leftover adapters that mirror product `gis/vista` without linking
`SmtScene` / `LP3DRENDERDEVICE`:

| Unit | Role |
| --- | --- |
| `dem_height_field` | Thin `DemRaster` shell + sample paths + label declutter helpers |
| `dem_to_world` | `seed_dem_height_field_into_world` |
| `coord` | leftover Y-up ↔ GIS envelope / `attach_gis_aabb` |

**DLL:** `vista_sources` → **`gis.dll`** (`GIS_EXPORT`).  
**Render seed / primitives:** `legacy/render/scene3d/{seed,primitive}`.

```cpp
#include "legacy/gis/vista/dem_height_field.h"
#include "legacy/gis/vista/dem_to_world.h"
#include "legacy/gis/vista/coord.h"
```

Namespace remains `render` for leftover call-site continuity.
