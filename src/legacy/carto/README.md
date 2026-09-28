<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/carto`

Leftover cartographic POD: `SmtStyle` / pen / brush / annotation / symbol,
`StyleManager`, and viewport/windowport helpers in `style_api`.

**Not** MapLibre Style JSON — that lives in [`../../gis/present/style/`](../../gis/present/style/).
**Not** GDI `MapCarto2d` paint — that lives under `legacy/render/rhi2d/impl/gdi/carto/`.

Axis-aligned extents are [`gis/model/envelope.h`](../../gis/model/envelope.h) (`gis::Envelope`).

## Include

```cpp
#include "legacy/carto/style.h"
#include "legacy/carto/stylemanager.h"
#include "legacy/carto/style_api.h"
```

Namespace remains `base` for these types (ABI / leftover call-site continuity).
Export macro stays `BASE_EXPORT` / `BASE_EXPORTS`.

## DLL

Sources are `carto_sources` → **`//src/base:base`** (product base DLL).
