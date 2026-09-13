<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# BCGControlBar Pro

Commercial UI stack used by `src/ui/mfc_ex`, `src/ui/gui`, `src/ui/xambox`, and `src/app` (`#include <BCGCBProInc.h>`). **Do not** replace it with VS Feature Pack `CMFC*`, WinUI, or ATL-only controls.

## Point GN at an install

Expected: this directory (or a junction) contains `BCGCBProInc.h`, or `include/BCGCBProInc.h`, or `BCGCBPro/BCGCBProInc.h`, plus the matching `.lib` under `lib/` / `lib/x64`.

```bat
mklink /J third_party\bcg C:\path\to\BCGControlBarPro
```

Or set `BCG_ROOT` / `BCGCBPRO` and run `python build\tools\find_bcg.py`, then junction here.

GN arg if the tree is elsewhere:

```bat
gn gen out --args="bcg_root=\"//third_party/bcg\""
```

Do not check a pirated BCG tree into git.

---

**最后更新：** 2026-09-13
