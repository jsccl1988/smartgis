<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/math`

Scene math (Eigen-backed POD). **Not** in `base.dll`. Definitions live in **`namespace base`** (no `base::math`); leftover **`namespace render`** aliases remain until call-site cutover. Internals that are not part of the POD API sit in **`base::detail`**.

| Layer | Path | Owns |
| --- | --- | --- |
| scalar | `scalar/` | `kPi` / `kEpsilon` / `deg_to_rad` |
| linear | `linear/` | `Vector2/3/4`, `Matrix`, `Quat`, `Point2`, `LpToDp2` |
| traits | `traits/` | `base::vector_traits` / `vector_like` |
| geom | `geom/` | `Aabb` / `Obb` / `Plane` / `Ray` / `Frustum` / cull enums |
| xform | `xform/` | `Transform` / `TransformStack` / `lerp` / `slerp` |
| simd | `simd/` | batch normalize / transform (`base_math_simd`) |
| detail | `detail/` | Eigen `Map` aliases (`detail::EigenVec*`) |

Preferred include: `#include "base/math/math.h"`. Layered paths for new TUs (`base/math/linear/vector.h`). There are **no** root-level `vector.h` shims.

GN: `//src/base/math:linear` (headers) · `:bounds` (geom cpp) · `:math` (facade + simd).

## Naming

| Kind | Rule | Examples |
| --- | --- | --- |
| Include guards | `BASE_MATH_<LAYER>_<STEM>_H_` | `BASE_MATH_LINEAR_VECTOR_H_` |
| Types | `PascalCase` | `Vector3`, `LpToDp2`, `Point2f` |
| Functions / methods | `snake_case` | `deg_to_rad`, `transform_xy`, `length` |
| Constants | `kCamelCase` | `kPi`, `kEpsilon`, `kInvalidCoord` |
| Enumerators | `enum class` + `kName` | `CullResult::kVisible` |
| Private members | `snake_case` + `_` | `TransformStack::stack_` |
| Eigen maps | `eigen()` for full storage; `xyz()` only on `Vector4` (drops `w`) | |
| SIMD GN / define | `base_math_simd` / `BASE_MATH_SIMD` | default off |

Leftover **public POD fields** stay as shipped (`vcMin`, `m_vcN`, `_11.._44`, `fA0`). Do not rename them in this tree.

Point / rect typedefs: new TUs use `Point2f` / `Rect2f` (and `l`/`d` siblings). `fPoint` / `lRect` / `dbfPoint` are leftover GDI spellings of the same types.

**最后更新：** 2026-10-05
