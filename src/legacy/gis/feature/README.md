<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/gis/feature`

Device-free OGR → CPU `FeatureMesh` tessellation for leftover
`SmtGeoObject` (`scene3d/primitive/feature`). Mirrors the vista split:
geometry prep lives in **`gis.dll`**; VB upload stays in `legacy_render`.

| Unit | Role |
| --- | --- |
| `mesh.h` | `FeatureVertex` / `FeatureMesh` / height sample typedef |
| `tess_map.cc` | Map-frame (lon/lat, optional DEM drape, stroke/fill RGB) |
| `tess_world.cc` | World-frame leftover 3D coords + `OGRTriangulatedSurface` |
| `leftover_feature.*` | leftover `SmtFeature` PascalCase wrap + `leftover_append_feature` |
| `leftover_copy_layer.*` | `copy_layer(SmtLayer*)` / `SmtRasterLayer*` (OGR copy stays product) |

**DLL:** `feature_sources` → **`gis.dll`** (`GIS_EXPORT`).  
**Forbidden:** `SmtScene` / `LP3DRENDERDEVICE`.

```cpp
#include "legacy/gis/feature/mesh.h"
```

Namespace remains `render` for leftover call-site continuity.
