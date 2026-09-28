<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/carto`

Cartographic POD: `Envelope`, `SmtStyle` / pen / brush / annotation / symbol, `StyleManager`.

**Not** MapLibre Style JSON — that lives in [`../../gis/style/`](../../gis/style/) (`gis::style`).

## Include

```cpp
#include "base/carto/envelope.h"
#include "base/carto/style.h"
```

Namespace remains `base` for these types (ABI/call-site continuity). Export macro stays `STYLE_EXPORT` / `STYLE_EXPORTS`.

## DLL

Sources are `carto_sources` → **`//src/base:base`** (product base DLL, `dll_stem=base`).

They are **not** linked into `gis.dll`: `//src/gis/kernel/geo` (inside that DLL) includes `Envelope`, so `gis.dll` depends on platform. Moving carto into `gis.dll` would reverse that.
