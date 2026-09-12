<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# mogu → smartgis 工程管理对照

Source of truth: WSL `/home/ccl/dev/src/mogu`。本仓工程入口是 **GN**，不是 sln。

| mogu | smartgis | 状态 |
| --- | --- | --- |
| `build.sh` + `m`/`te`/`a`/`b` | 根目录 `build.bat`（同别名） | 已建 |
| `BUILD.gn` 根组 | 同 | 已建 |
| `.gn` → `//build/BUILDCONFIG.gn` | 同（Windows toolchain 经 mgis 适配） | 已建 |
| 模块树在仓库根（`base/` …） | 产品在 **`src/`**（已从已删除的 `branches/SmartGis.1.1.vs08/src` 迁出） | 已迁 |
| `third_party/` | `third_party/` | 已建 |
| `out/` + `out/build.log` | 同 | 已建 |
| Bazel / `.install` | **不搬** | 跳过 |
| — | `vs2008/` / `branches/` | **已删除**；入口只有 GN |

---

**最后更新：** 2026-09-13
