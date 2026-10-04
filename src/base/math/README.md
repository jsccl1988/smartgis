<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/math`

Scene math (Eigen-backed POD). **Not** in `base.dll`. Namespace **`base`**; leftover `render::` aliases remain until call-site cutover.

| Layer | Path | Owns |
| --- | --- | --- |
| scalar | `scalar/` | `kPi` / `kEpsilon` / `deg_to_rad` |
| linear | `linear/` | `Vector2/3/4`, `Matrix`, `Quat`, `Point2`, `LpToDp2` |
| traits | `traits/` | `base::vector_traits` / `vector_like` |
| geom | `geom/` | `Aabb` / `Obb` / `Plane` / `Ray` / `Frustum` / cull enums |
| xform | `xform/` | `Transform` / `TransformStack` / `lerp` / `slerp` |
| simd | `simd/` | batch normalize / transform (`smt_render_math_simd`) |
| detail | `detail/` | Eigen `Map` aliases |

Preferred include: `#include "base/math/math.h"`. Layered paths for new TUs (`base/math/linear/vector.h`). Root `vector.h` etc. are leftover cutover aliases.

GN: `//src/base/math:linear` (headers) · `:bounds` (geom cpp) · `:math` (facade + simd).

**最后更新：** 2026-10-05
