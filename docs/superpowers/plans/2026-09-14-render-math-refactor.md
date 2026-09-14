<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# render/math 重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **本轮用户指令：** 设计已批准，「并行执行无需再确认」；在 **master** 上改；**不要 commit**（除非用户另行要求）。

**Goal:** 将 `src/render/math` 从 `mathlib_3d.h` 巨头硬切为拆分的 snake_case Eigen 适配库，迁完 in-tree 调用方，并分期落地 Frustum / TransformStack / 插值 / 可选 AVX2 SIMD。

**Architecture:** 薄 POD（`.x/.y/.z`、Matrix `_11.._44` RowMajor）+ `Eigen::Map`；公开命名空间仅 `render`；内部 `render::detail`。bounds 相交单向化（Ray 测盒，删除盒侧镜像）。SIMD 仅 float32 + 编译期 AVX2，否则标量；GN 默认关。

**Tech Stack:** C++23、Eigen（`//third_party:eigen`）、GN/Ninja（`out/` only）、现有 `test()` 目标风格。

## Global Constraints

- 工作目录 `C:/Dev/src/gis/smartgis`，只在 **`master`** 改；不要开分支、不要 push。
- **不要 commit**，除非用户明确要求。
- Spec：`docs/superpowers/specs/2026-09-14-render-math-refactor-design.md`。
- 禁止：glm 替换 `render/math`、Qt、旧名 shim、第二份 Eigen。
- Copyright：`Copyright (c) 2026 The Mogu Authors.`；源码注释英文；文档中文。
- 函数 `snake_case`；公共命名空间 ≤2（`render` / `render::detail`）。
- 查找代码先 CBM（project=`smartgis`），禁止全库 Grep 作第一步。
- SIMD 基线：**仅 float32 + 编译期 AVX2，否则标量**；`smt_render_math_simd` 默认 false。

## File map

| Path | Responsibility |
| --- | --- |
| `src/render/math/constants.h` | `kPi`、`deg_to_rad`、`PlaneSide`、`CullResult` |
| `src/render/math/vector.h` | Vector2/3/4 + free `dot`/`cross` |
| `src/render/math/matrix.h` | Matrix + `transform_point`/`transform_vector` |
| `src/render/math/quat.h` | Quat |
| `src/render/math/aabb.h` + `aabb.cpp` | Aabb（无 Ray 镜像 intersects） |
| `src/render/math/obb.h` + `obb.cpp` | Obb（无 Ray 镜像 intersects） |
| `src/render/math/plane.h` + `plane.cpp` | Plane |
| `src/render/math/ray.h` + `ray.cpp` | Ray（含对 Aabb/Obb 的 intersects） |
| `src/render/math/frustum.h` + `frustum.cpp` | 切片 2 |
| `src/render/math/transform.h` | Transform + TransformStack |
| `src/render/math/interpolate.h` | lerp/slerp/nlerp |
| `src/render/math/simd.h` + `simd_avx2.cc` | 切片 3 |
| `src/render/math/math.h` | 聚合头 |
| `src/render/math/math_test.cc` | 单测骨架 |
| `src/render/math/BUILD.gn` | targets + SIMD 开关 |
| 删除 | `mathlib_3d.h`、`mathlib.h` |
| 调用方 | `legacy/render/**`、`algorithm/tin/**`、`algorithm/geo/vector_traits.h` |
| docs | `docs/build/src-layout.md`、`src/render/README.md` |

**明确不做清单：**

- 不引入 glm。
- 不改 `sdb` 自有 AABB。
- 不做运行时 CPUID / 多 ISA dispatch。
- 不保留 `GetLength` / `RotaX` 等 shim。

---

### Task 1: MVP — 新头文件骨架 + constants/vector

**Files:**
- Create: `src/render/math/constants.h`
- Create: `src/render/math/vector.h`
- Modify: `src/render/math/BUILD.gn`（先挂上 headers 源列表若需要；header-only 可仅 public_deps）

**Interfaces:**
- Produces: `render::kPi`、`deg_to_rad`、`rad_to_deg`、`PlaneSide`、`CullResult`；`Vector2/3/4` 含 `set`/`length`/`length_squared`/`normalize`/`negate`/`cross`/`dot`/`angle_with`；free `dot`/`cross`；**无** `operator*(Vector,Vector)`

- [ ] **Step 1: 写 `constants.h`**

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#pragma once
namespace render {
inline constexpr double kPi = 3.14159265;
inline constexpr double kHalfPi = 1.5707963;
inline constexpr double kTwoPi = 6.2831853;
inline constexpr float kEpsilon = 0.00001f;
inline constexpr float kGravity = -32.174f;
constexpr float deg_to_rad(float a) { return static_cast<float>(kPi / 180.0 * a); }
constexpr float rad_to_deg(float a) { return static_cast<float>(180.0 / kPi * a); }
// Temporary macros for leftover call sites (northarray); prefer functions.
#ifndef DEG2RAD
#define DEG2RAD(a) (::render::deg_to_rad(static_cast<float>(a)))
#endif
#ifndef RAD2DEG
#define RAD2DEG(a) (::render::rad_to_deg(static_cast<float>(a)))
#endif
enum class PlaneSide { kFront = 0, kBack = 1, kPlanar = 2 };
enum class CullResult { kClipped = 3, kCulled = 4, kVisible = 5 };
}  // namespace render
```

- [ ] **Step 2: 写 `vector.h`（从 `mathlib_3d.h` 迁出并硬切方法名）**

关键：删除 `float operator*(const VectorN&)`；增加 `float dot(const VectorN&) const` 与 free `dot`。`CrossProduct` → `cross`。`GetLength` → `length`。保留 `operator*(float)`。

- [ ] **Step 3: 本地静态检查**

确认无 `operator*(const Vector3&)` 点积 overload；`dimension` / `coordinate_type` 仍可供 `vector_traits` 使用。

---

### Task 2: MVP — matrix / quat / 聚合头

**Files:**
- Create: `src/render/math/matrix.h`
- Create: `src/render/math/quat.h`
- Create: `src/render/math/math.h`

**Interfaces:**
- Produces: `Matrix::{identity,rotate_x,rotate_y,rotate_z,rotate_euler,rotate_axis,translate,scale,set_translation,translation,set_perspective,look_at,billboard,transpose_of,inverse_of,transform_point,transform_vector}`；`Quat::{from_euler,to_euler,to_matrix,normalize,conjugate,magnitude,rotate_vector}`

- [ ] **Step 1: `matrix.h`**

`Matrix::operator*(const Vector4&)` 实现改为调用 `transform_point`（带透视除法）。新增 `transform_vector`（仅 3×3 旋转缩放，无平移/除法）。`RotaX` → `rotate_x` 等。内部点积改用 `dot()`。

- [ ] **Step 2: `quat.h`**

`MakeFromEuler` → `from_euler`；`GetEulers` → `to_euler`；`GetMatrix` → `to_matrix`；`Conjugate` → `conjugate`（按值入参可改为 const ref）。

- [ ] **Step 3: `math.h` 聚合**

```cpp
#include "render/math/constants.h"
#include "render/math/vector.h"
#include "render/math/matrix.h"
#include "render/math/quat.h"
#include "render/math/aabb.h"
#include "render/math/obb.h"
#include "render/math/plane.h"
#include "render/math/ray.h"
#include "render/math/frustum.h"
#include "render/math/transform.h"
#include "render/math/interpolate.h"
#include "render/math/simd.h"
```

（切片未落地前可条件 include 或先提供空头。）

---

### Task 3: MVP — bounds 拆头 + 单向化 + cpp 改名

**Files:**
- Create: `aabb.h` `obb.h` `plane.h` `ray.h`
- Modify: `aabb.cpp` `obb.cpp` `plane.cpp` `ray.cpp`
- Modify: `BUILD.gn`

**Interfaces:**
- Produces: snake_case 方法；`Ray::intersects(Aabb/Obb/...)` 保留实现；**删除** `Aabb::intersects(Ray)` / `Obb::intersects(Ray)`
- Consumes: vector/matrix 新头

- [ ] **Step 1: 声明迁到独立头，方法改名**

`Intersects`→`intersects`，`Contains`→`contains`，`Cull`→`cull`，`GetPlanes`→`get_planes`，`DeTransform`→`de_transform`，`Distance`→`distance`，`Classify`→`classify` 返回 `PlaneSide`，`Clip`→`clip`。

- [ ] **Step 2: 单向化**

从 `aabb.h/cpp`、`obb.h/cpp` 删除对 `Ray` 的 `intersects`；调用方改为 `ray.intersects(aabb, &t)`。

- [ ] **Step 3: cpp 内点积与宏**

`(a * b)` 点积 → `dot(a,b)`；`FRONT`/`CULLED` → `PlaneSide`/`CullResult`；`_fabs` → `std::fabs`；`#include "render/math/math.h"` 或细头。

- [ ] **Step 4: 更新 `BUILD.gn`**

`bounds` sources 增加 `frustum.cpp`（切片 2 时可先空实现）；`math` 保持 header-only + Eigen。

---

### Task 4: MVP — 删除 mathlib* + 迁 include/调用方

**Files:**
- Delete: `mathlib_3d.h`、`mathlib.h`
- Modify: 所有原 `#include "render/math/mathlib_3d.h"` 文件
- Modify: `algorithm/geo/vector_traits.h` → `#include "render/math/vector.h"`（或 `math.h`）
- Modify: `legacy/render/render3d/base.h` 等

**Interfaces:**
- Consumes: Task 1–3 API

- [ ] **Step 1: 替换 include**

```text
render/math/mathlib_3d.h → render/math/math.h
render/math/mathlib.h    → render/math/math.h
```

算法侧可细到 `vector.h`。

- [ ] **Step 2: legacy_render 方法改名（批量）**

| 调用 | 替换为 |
| --- | --- |
| `.GetLength()` | `.length()` |
| `.GetSqrLength()` | `.length_squared()` |
| `.Normalize()` | `.normalize()` |
| `.Negate()` | `.negate()` |
| `.CrossProduct(` | `.cross(` |
| `.Identity()` | `.identity()` |
| `.Intersects(` | `.intersects(` 或改为 `ray.intersects(aabb,` |
| `.Contains(` | `.contains(` |
| `.Set(`（Vector/Ray） | `.set(` |

注意：**不要**改 `SmtGLRenderDevice::SetPerspective` 等设备 API。

- [ ] **Step 3: 验证 tin / vector_traits 仅 include 变更即可编译语义不变**

---

### Task 5: MVP — 单测骨架 + docs 最小更新

**Files:**
- Create: `src/render/math/math_test.cc`
- Modify: `src/render/math/BUILD.gn`（`test("math_test")`）
- Modify: `docs/build/src-layout.md`、`src/render/README.md`

**Interfaces:**
- Produces: 可运行的 `math_test`（至少向量 dot/cross、Ray-Aabb intersects）

- [ ] **Step 1: 写失败/通过测试**

```cpp
// math_test.cc — 无 gtest 依赖时用 assert + main，对齐 tin_*_test 风格
#include "render/math/math.h"
#include <cassert>
#include <cmath>
int main() {
  render::Vector3 a(1, 0, 0), b(0, 1, 0);
  assert(std::fabs(dot(a, b)) < 1e-6f);
  assert(std::fabs(a.cross(b).z - 1.f) < 1e-6f);
  render::Aabb box(render::Vector4(-1,-1,-1), render::Vector4(1,1,1));
  render::Ray ray;
  ray.set(render::Vector4(0,0,-5), render::Vector4(0,0,1));
  float t = 0;
  assert(ray.intersects(box, &t));
  return 0;
}
```

- [ ] **Step 2: docs**

`src-layout.md`：去掉「mathlib_3d」表述，写明拆分头 + snake_case 硬切。  
`src/render/README.md`：`math/` 行注明 Eigen、禁 glm、聚合头 `math.h`。

- [ ] **Step 3: 编译相关 target（若环境可用）**

```bat
build.bat te //src/render/math:math_test
```

或 `ninja -C out math_test`（以仓库 `build.bat` 为准）。

---

### Task 6: 切片 2 — Frustum + TransformStack + interpolate

**Files:**
- Create/Modify: `frustum.h` `frustum.cpp` `transform.h` `interpolate.h`
- Modify: `math_test.cc`（追加用例）

**Interfaces:**
- Produces: `Frustum::from_view_proj`、`intersects(Aabb/Obb)`、`classify`；`TransformStack::{push,pop,clear,translate,rotate_axis,scale,matrix}`；`lerp`/`slerp`/`nlerp`

- [ ] **Step 1: `interpolate.h`**

```cpp
template <typename T>
T lerp(const T& a, const T& b, float t) { return a * (1.f - t) + b * t; }
Quat nlerp(const Quat& a, const Quat& b, float t);
Quat slerp(const Quat& a, const Quat& b, float t);
```

- [ ] **Step 2: `transform.h`**

栈深度可用 `std::vector<Matrix>`；`push` 复制顶；`translate`/`scale`/`rotate_axis` 右乘顶矩阵。

- [ ] **Step 3: `Frustum`**

从 VP 提取六平面（可参考 `gl_rdev_misc.cpp` 逻辑，但用 `Plane` 类型）；`intersects(Aabb)` 委托 `Aabb::cull` 或独立实现。

- [ ] **Step 4: 测试**

`lerp(0,10,0.5)==5`；`nlerp` 单位四元数端点；Frustum 包住原点盒。

---

### Task 7: 切片 3 — SIMD GN 开关 + AVX2/标量

**Files:**
- Create: `simd.h` `simd_avx2.cc`
- Modify: `BUILD.gn`、根/`smartgis.gni` 若需 declare_args
- Modify: `math_test.cc`（标量路径必过；SIMD 开时对比）

**Interfaces:**
- Produces: `normalize_batch(std::span<Vector3>)`、`transform_points_batch(const Matrix&, std::span<Vector3>)`；无 AVX2 或开关关 → 标量循环

- [ ] **Step 1: GN**

```gn
declare_args() {
  smt_render_math_simd = false
}
```

`smt_render_math_simd` 为 true 时给 `simd_avx2.cc` 加 `/arch:AVX2`（MSVC）或 `-mavx2`，并 define `SMT_RENDER_MATH_SIMD=1`。

- [ ] **Step 2: 实现**

`#if defined(SMT_RENDER_MATH_SIMD) && defined(__AVX2__)`（或 MSVC `_MSC_VER` + `/arch:AVX2`）走 AVX2；否则标量。

- [ ] **Step 3: 默认关着编过 `math_test`**

---

## 验证清单（整主题）

1. 仓库内无 `mathlib_3d.h` / `mathlib.h` 引用。
2. 无 `GetLength`/`CrossProduct`/`RotaX` 等对 `render::` 类型的旧调用。
3. `math_test` 与 `tin_*_test` 可运行。
4. docs 最小更新已写入。
5. **未** git commit（除非用户要求）。

## Spec 覆盖自检

| Spec 项 | Task |
| --- | --- |
| 拆文件 + 硬切 + 单向化 + 调用方 | 1–4 |
| 单测骨架 + docs | 5 |
| Frustum / Transform / interpolate | 6 |
| SIMD float32 AVX2 可选 | 7 |
| 禁 glm / Eigen only / RowMajor | Global + Task 2 |
| 删 `v*v` / transform_point|vector | Task 1–2 |
