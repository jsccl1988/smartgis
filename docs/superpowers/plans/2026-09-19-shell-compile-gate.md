<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP5 Shell compile gate — Implementation Plan

> **For agentic workers:** follow checkboxes; stay on `master`; no commit unless asked.

**Goal:** 钉死默认壳 / `src_all` 不拉 leftover 聚合；文档写清 opt-in；GN `assert_no_deps` + group 分离。  
**Spec:** [`../specs/2026-09-19-shell-compile-gate-design.md`](../specs/2026-09-19-shell-compile-gate-design.md)

## Constraints

- 可改：根/`BUILD.gn`、`src/BUILD.gn`、`src/app/views/BUILD.gn`（闸门）、`docs/build/*`、根 README、伞状 Child 表。
- **禁止**改 leftover 业务；不大改 scene3d；保留 opt-in `legacy_*`。
- 中文文档；源码注释英文；copyright 2026。

---

### Task 1: Living design + 伞状 Child 行

**Files:**
- Create: `docs/superpowers/specs/2026-09-19-shell-compile-gate-design.md`
- Create: `docs/superpowers/plans/2026-09-19-shell-compile-gate.md`（本文）
- Modify: `docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md` Child 表 SP5 行

**Steps:**
1. [x] Spec Status active；含成功标准模板各节。
2. [x] 伞状 Child 表：`SP5` → shell-compile-gate design + plan。

---

### Task 2: GN 闸门

**Files:**
- Modify: `src/BUILD.gn`（`assert_no_deps` + 注释）
- Modify: `BUILD.gn`（`legacy_all`、`test_shell`、注释）
- Modify: `src/app/views/BUILD.gn`（Views `assert_no_deps`；允许 dem static）

**Steps:**
1. [x] `src_all`：`assert_no_deps` 禁止 leftover DLL / `legacy_*_all` / MFC app / `ui_legacy`。
2. [x] `views` / `SmartGisViews`：禁止 `legacy_render` DLL 与 tool/app/ui leftover；**不**禁 `dem_height_field_static`。
3. [x] 根 `group("legacy_all")`：聚合 `legacy_render_all` + `legacy_tool_all` + `legacy_app_all`（gated）+ `ui_legacy`。
4. [x] 根 `group("test_shell")`：终局/壳相关 test，不含 leftover paint / dem_stereo / MFC app。

---

### Task 3: 文档

**Files:**
- Modify: `docs/build/src-layout.md`（Shell 编译闸门专节）
- Modify: 根 `README.md`（必要时 +「最后更新」）
- Optional: `docs/README.md` 索引一行

**Steps:**
1. [x] 写清默认 vs opt-in 命令表。
2. [x] 标明 Views → `dem_height_field_static` 例外（SP4 收口）。

---

### Task 4: 验证

**Steps:**
1. [x] `build.bat app`（或 `views`）绿。
2. [x] `build.bat` / `ninja -C out all` 或等价编通 `src_all`。
3. [x] 跑受影响壳相关 test（`content_feature_attrs_test` / `content_catalog_layers_test` / `map_scene_test` / `scene3d_controller_test` / `views_unittests`）。
4. [x] `gn gen` 确认 `assert_no_deps` 未误伤（无 unexpected assert）；`build.bat legacy_all` opt-in 仍可用。
