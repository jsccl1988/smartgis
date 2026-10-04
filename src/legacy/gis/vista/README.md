<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/gis/vista`

Leftover adapters that mirror product `src/vista` without linking
`SmtScene` / `LP3DRENDERDEVICE`:

| Unit | Role |
| --- | --- |
| `dem_height_field` | Thin `DemRaster` shell + sample paths + label declutter helpers |
| `dem_to_world` | `seed_dem_height_field_into_world` |
| `coord` | leftover Y-up ↔ GIS envelope / `attach_gis_aabb` |

**DLL:** `vista_sources` → **`vista.dll`** (`VISTA_EXPORT`). Disk path stays `src/legacy/gis/vista`.  
These adapters are **not** the **Scenic** leftover render engine (`src/legacy/render`).  
**Render seed / primitives:** `legacy/render/scene3d/{seed,primitive}` (Scenic).

```cpp
#include "vista/world/terrain/dem/dem_height_field.h"
#include "vista/world/terrain/dem/dem_to_world.h"
#include "vista/world/coord.h"
```

Namespace remains `render` for leftover call-site continuity.
