<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/sdb/carto`

Cartographic POD: `Envelope`, `SmtStyle` / pen / brush / annotation / symbol, `StyleManager`.

**Not** MapLibre Style JSON — that lives in [`../style/`](../style/) (`sdb::style`).

## Include

```cpp
#include "sdb/carto/envelope.h"
#include "sdb/carto/style.h"
```

Namespace remains `base` for these types (ABI/call-site continuity). Export macro stays `STYLE_EXPORT` / `STYLE_EXPORTS`.

## DLL

Sources are `carto_sources` → **`//src/base:base`** (product platform DLL, transitional `dll_stem=base`).

They are **not** linked into `sdb.dll`: `algorithm/geo` includes `Envelope` while `sdb` already depends on `algorithm`, so putting carto in the sdb DLL would cycle.

When the platform stem becomes `platform`, update `#pragma comment(lib, …)` in carto headers in the same change.
