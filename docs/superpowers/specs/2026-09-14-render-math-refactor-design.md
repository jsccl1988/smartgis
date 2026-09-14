<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# `src/render/math` 增强并重构精简冗余

**Date:** 2026-09-14  
**Topic:** 场景数学库硬切 `snake_case`、文件拆分、bounds 相交单向化，并分期落地 Frustum / TransformStack / 插值 / SIMD  
**Backend:** Eigen only（禁止 glm 替换整模块；FlyCube 自带 glm 仅留 RHI 边界）

## Goal

1. 把 leftover 适配层收成**可维护、snake_case、两层命名空间 `render`** 的场景数学库。
2. **C 硬切**：无 PascalCase / `GetLength` / `RotaX` 等旧名转发；同变更同步所有 in-tree 调用方（含 `legacy_render`、`algorithm/tin`、`vector_traits`）。
3. 在精简基础上补齐：Frustum、Transform 栈、插值、SIMD 批量（可选路径）。
4. 保持与 `docs/build/src-layout.md`、algorithm OSS、model-render 设计一致：Eigen = 场景数学，不是第二套 OGC 几何核。

## Non-goals

- 不用 glm / 第二份 Eigen / 自研线性代数核替换。
- 不把 OGR/GEOS 谓词搬进 `render/math`。
- 不重写 `sdb` 自有 AABB 查询（除非日后显式统一）。
- 不在本主题内做 GPU compute 视锥（实例极多时再考虑）。
- 不为硬切保留长期兼容 shim。

## 现状与冗余（核实摘要）

| 路径 | 角色 |
| --- | --- |
| `mathlib_3d.h` (~747 行) | 巨头：`Vector2/3/4`、`Matrix`、`Ray`/`Plane`/`Obb`/`Aabb`/`Quat`、常量/宏 |
| `mathlib.h` | 仅 include + `geo::Vector = render::Vector2` |
| `aabb.cpp` / `obb.cpp` / `plane.cpp` / `ray.cpp` | bounds 实现 |
| `BUILD.gn` | `math`（头/Eigen）+ `bounds`（上述 cpp） |

已是 **Eigen-backed leftover 适配**：`Eigen::Map` 包 `.x/.y/.z` 与 D3D 风格 `_11.._44`（`EigenMat4` = **RowMajor**）。

主要冗余：

1. Vector2/3/4 方法高度复制。
2. `operator*(Vector, Vector)` = 点积（与现代直觉冲突）→ 改为 `dot()`。
3. `Ray::Intersects(Aabb/Obb)` 与 `Aabb/Obb::Intersects(Ray)` 双向冗余。
4. leftover 宏（`NULL`/`FRONT`/…）、`Rota*` 与 `Rotate` 重叠、点/方向变换未区分（`Matrix * Vector4` 强制透视除法）。

硬切调用面：`legacy_render`（经 `base.h`）、`algorithm/tin` + `geo/vector_traits`；几乎无独立 math 单测。

## 推荐实现路线

**薄 POD + Eigen Map + 文件拆分（已批准）**

- 保留 `float x,y,z` / `_11.._44` 布局；内部继续 `eigen()` Map。
- 函数 **snake_case**；删除 `v*v` 点积 overload。
- bounds **单向化**。
- **Matrix**：继续 **RowMajor + `_11.._44`**。
- 不引入 glm。

分期：

| 切片 | 内容 |
| --- | --- |
| **MVP** | 文件拆分 + C 硬切 + 相交单向化 + 迁完调用方 + 单测骨架 |
| **切片 2** | Frustum；TransformStack；`lerp` / `slerp` / `nlerp` |
| **切片 3** | 可选 float32 SIMD；**仅 float32 + 编译期 AVX2，否则标量**；GN 可选，默认标量 |

## 目标 API 形状

**命名空间：** `render`（公开两层止于此）；内部 `render::detail`。

| 类型 | 要点 |
| --- | --- |
| `Vector2` / `Vector3` / `Vector4` | `float` 分量；`eigen()` Map；`length` / `length_squared` / `normalize` / `normalized`；`dot` / `cross`；**禁止** `v*v` 点积 |
| `Matrix`（4×4） | 保留 `_11.._44`，RowMajor Map；`identity` / `transpose_of` / `inverse_of`；`rotate_x/y/z` / `rotate_axis` / `translate` / `scale`；`look_at` / `set_perspective` / `billboard`；区分 **`transform_point` / `transform_vector`** |
| `Quat` | `from_euler` / `to_euler` / `to_matrix` / `normalize` / `conjugate` / `rotate_vector` |
| `Aabb` / `Obb` / `Plane` / `Ray` | 方法硬切 snake_case；字段名可暂留 leftover（`vcMin` 等）二期再改 |
| `Frustum` | 由 VP 矩阵或六平面构建；`intersects` / `classify` Aabb/Obb |
| `Transform` / `TransformStack` | TRS；`push`/`pop`/`load`/`mult`；`matrix()` |
| 插值 | free：`lerp`、`slerp`、`nlerp` |
| SIMD | `render::detail` 或 `simd.h` 批量 API；公开入口接受 `std::span` |

**Bounds 相交单向化：**

- `Ray`：测三角形 / 平面 / Aabb / Obb（实现唯一归属）。
- `Aabb` / `Obb`：测点 / 盒 / 三角；**删除**对 Ray 的镜像 `intersects`。
- `Plane`：测点分类、与其它平面/盒相交；裁剪结果用 `enum class`，不用宏。

**常量：** `kPi` 等 `constexpr`；保留 `deg_to_rad` / `rad_to_deg`（替换 `DEG2RAD`/`RAD2DEG` 宏，可提供兼容宏一期）。删除 `NULL`/`FRONT` 等宏。

**`geo::Vector`：** 迁到 `algorithm/geo` 显式 using，或由 `math.h` 薄提供；避免隐藏依赖。

## 文件布局

```
src/render/math/
  BUILD.gn              # math（头）; bounds（cpp）; 可选 math_simd
  math.h                # 聚合头（新代码首选）
  constants.h
  vector.h              # Vector2/3/4 + free dot/cross/length
  matrix4.h             # Matrix 实现（避免与 Eigen Matrix.h 冲突）
  matrix.h              # 薄别名 → matrix4.h
  quat.h
  aabb.h / aabb.cpp
  obb.h  / obb.cpp
  plane.h / plane.cpp
  ray.h  / ray.cpp
  frustum.h / frustum.cpp
  transform_stack.h     # Transform + TransformStack
  transform.h           # 薄别名 → transform_stack.h
  interpolate.h
  simd.h / simd.cc      # 切片 3；GN smt_render_math_simd 默认关
  math_test.cc
```

`mathlib_3d.h` / `mathlib.h` 已删除；include 改为 `render/math/math.h`。`math_headers` 不得把本目录加入 `include_dirs`。

## 硬切迁移策略

1. 落地新头 + 更新 `BUILD.gn`。
2. 全库替换 include：`mathlib_3d.h` / `mathlib.h` → `render/math/math.h`。
3. 符号改名（示例）：

| 旧 | 新 |
| --- | --- |
| `GetLength` | `length` |
| `GetSqrLength` | `length_squared` |
| `CrossProduct` | `cross` |
| `Normalize` / `Negate` / `Set` | `normalize` / `negate` / `set` |
| `Identity` | `identity` |
| `RotaX/Y/Z` / `RotaArbi` | `rotate_x/y/z` / `rotate_axis` |
| `MakeFromEuler` | `from_euler` |
| `Intersects` / `Contains` / `Cull` | `intersects` / `contains` / `cull` |
| `v * v`（点积） | `dot(v, v)` / `a.dot(b)` |
| `m * v`（带透视除法） | `transform_point`；方向用 `transform_vector` |

4. `legacy/render/render3d/base.h`：改 include；可暂留 `using namespace render`。
5. `vector_traits` / tin：改 include；类型名 `Vector3` 可保留。
6. 无双名时期；以编译为准一次过。

## 各增强最小可用面

### (1) Frustum（切片 2）

- `Frustum::from_view_proj(const Matrix&)` 或 `from_planes`
- `intersects(const Aabb&)` / `intersects(const Obb&)`
- `classify` → outside / intersect / inside（`enum class`）
- 与 `Aabb::cull(Plane*)` 对齐：Frustum 持有 6 平面，可委派

### (2) TransformStack（切片 2）

- `push` / `pop` / `clear`
- `translate` / `rotate_quat` / `rotate_axis` / `scale`
- `matrix() const`；可选 `set_trs(t, r, s)`

### (3) interpolate（切片 2）

- `lerp(a, b, t)`（标量 / 向量）
- Quat：`nlerp`（快）、`slerp`（等弧）
- 不写通用样条

### (4) SIMD（切片 3）— 已定基线

- **仅 float32 + 编译期 AVX2，否则标量**（已批准，不做运行时 ISA dispatch）。
- GN arg（如 `smt_render_math_simd`），**默认关闭（标量）**。
- API 示例：`normalize_batch`、`transform_points_batch(Matrix, span<Vector3>)`。
- **不**改变标量默认路径语义；测试用 epsilon。

## 测试计划

| 位置 | 内容 |
| --- | --- |
| `src/render/math/*_test.cc` | 点积/叉积/归一化；矩阵 look_at 冒烟；Quat slerp 端点；Ray↔Aabb 单向 intersects；Frustum vs Aabb |
| 现有 `algorithm/tin/*_test` | 硬切后回归 |
| legacy | 依赖编译 + 手动 3D 冒烟 |

## 风险与回滚

| 风险 | 缓解 |
| --- | --- |
| leftover `v*v` 点积漏改 | 编译期删除该 overload；搜相关法线点积 |
| 透视除法改变方向向量 | 显式 `transform_point` / `transform_vector` |
| 大 PR 难审 | MVP 与切片 2/3 可逻辑分期，硬切必须在 MVP 一次做完 |
| SIMD 数值差 | epsilon；默认关 SIMD |
| 回滚 | git revert；无数据格式变更 |

## docs 联动

- `docs/build/src-layout.md`：更新 leftover/`mathlib_3d` 表述 → 新头与 API 策略。
- `src/render/README.md`：math 行注明拆分、Eigen、禁止 glm。
- 落地后：本 spec → `landed` 并归档；as-built 写入 `docs/build/` 或 README。
- 交叉引用：不改 algorithm/model-render 的 Eigen 结论，可加「API 已硬切」一句。

## 批准记录

- API：C 硬切，无旧名 shim。
- 后端：Eigen only；禁止 glm 替换。
- 形态：薄 POD + Eigen Map；Matrix RowMajor `_11.._44`。
- 增强 1–4 全要，按 MVP → 切片 2 → 切片 3 分期。
- SIMD 基线：**仅 float32 + 编译期 AVX2，否则标量**；GN 可选，默认标量。
- 点积：删除 `v*v`，改用 `dot()`；区分 `transform_point` / `transform_vector`。
