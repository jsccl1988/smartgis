<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# octree (unibn / jbehley)

MIT header-only octree for 3D radius / nearest-neighbor search.

| | |
| --- | --- |
| Upstream | https://github.com/jbehley/octree |
| Namespace | `unibn` |
| Header | `Octree.hpp` |
| Pin | `8e3927d48d5ce61f94aab6090d77b4434f87dc89` |
| Source tree | `third_party/.src/octree` (gitignored; `tools/fetch.py` / clone) |
| GN | `//third_party:octree` → `//third_party/octree:octree` |

Used only by `src/legacy/render/scene3d/index` adapters (`SmtSceneOctTree` / `SmtVertexOctTree`). Not a scene engine.

```cpp
#include "Octree.hpp"
// unibn::Octree<PointT>
```

Fetch:

```bat
py -3 third_party\tools\fetch.py --package octree
REM or:
git clone https://github.com/jbehley/octree.git third_party\.src\octree
git -C third_party\.src\octree checkout 8e3927d48d5ce61f94aab6090d77b4434f87dc89
```

---

**最后更新：** 2026-09-28
