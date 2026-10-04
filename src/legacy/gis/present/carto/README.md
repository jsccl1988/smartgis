<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/gis/present/carto`

Leftover cartographic POD: `SmtStyle` / pen / brush / annotation / symbol,
`StyleManager`, and viewport/windowport helpers in `style_api`.

Mirrors endgame `gis/present/carto` under the leftover tree until the POD is
retired or re-homed into product `gis/`.

**Not** MapLibre Style JSON — that lives in [`../../../../gis/carto/style/`](../../../../gis/carto/style/).
**Not** GDI `MapCarto2d` paint — that lives under `legacy/render/rhi2d/impl/gdi/`.

Axis-aligned extents are [`gis/envelope.h`](../../../../gis/envelope.h) (`gis::Envelope`).

## Include

```cpp
#include "legacy/gis/present/carto/style.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/gis/present/carto/style_api.h"
```

Namespace remains `base` for leftover call-site continuity.
Export macro is `GIS_EXPORT` / `GIS_EXPORTS` (product **`gis`** DLL).

## DLL

Sources are `carto_sources` → **`//src/gis:gis`** (`dll_stem=gis`).
