<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 代码规范 + mogu include + 破 ABI 全仓切断 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在一个逻辑变更集内，把 `src/` 产品 C++ 切到 mogu 式路径 include、`snake_case` + 两层命名空间，并打破 `Smt*` DLL/导出/插件 ABI；验收 `src_all` + app/views + 测试 + 映射文档。

**Architecture:** Include 根仅 `//src`；物理树不动。用 `docs/build/abi-rename-map.md` 作为单一映射源。多 agent **按不相交目录并行**机械改写；中间允许红构建；最后一人收口 GN/`config("legacy")` 并跑绿验收。不留转发头、不留旧导出名。

**Tech Stack:** GN/Ninja（`build.bat`）、MSVC C++23、现有 `smt_shared_library`、仓内插件 host。

**Spec:** [`docs/superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../specs/2026-09-13-code-style-include-abi-cutover-design.md)

## Global Constraints

- 只在 **`master`** 上改；不新开分支 / worktree 发布分支。
- Include：`#include "layer/module/file.h"`；禁止恢复模块级 `include_dirs` 救编译。
- ABI：旧 `Smt*` stem / `Export_Smt*` / 插件按旧文件名加载 **全部删除**。
- 函数 `snake_case`；类型 `PascalCase`；公共命名空间最多两层；注释英文。
- 版权头：新文件 / 触及文件的 Mogu 头改为 2026。
- 不引入 Qt；不改 `third_party/.src` 上游符号。
- 大爆炸：任务 2–8 不要求各自编绿；任务 9 必须全绿。

## File map（责任）

| 路径 | 责任 |
| --- | --- |
| `docs/build/abi-rename-map.md` | 旧→新：include、dll_stem、导出宏、命名空间、插件 stem |
| `build/BUILD.gn` (`config("legacy")`) | 删除按模块 `include_dirs` |
| `build/smartgis.gni` / 各 `src/**/BUILD.gn` | 新 `dll_stem`、导出相关 `defines` |
| `src/base/**`, `src/sys/**` | 路径 include + 命名/导出 |
| `src/algorithm/**` | 同上 |
| `src/sdb/**` | 同上 |
| `src/render/**`, `src/gpu/**` | 同上 |
| `src/net/**`, `src/tool/**` | 同上 |
| `src/ui/**`, `src/plugin/**`, `src/app/**`, `src/content/**` | 同上；插件加载表 |
| `testing/**` | 测试 include/符号跟随 |
| `docs/build/src-layout.md`, `src/README.md`, 根 `README.md` | 叙述与 DLL 名 |

---

### Task 1: 映射表与门禁脚本骨架

**Files:**
- Create: `docs/build/abi-rename-map.md`
- Create: `tools/cutover/check_flat_includes.py`（或 `tools/cutover/scan_abi_residuals.py`）
- Modify: `docs/build/src-layout.md`（文首加「进行中：ABI/include 切断，见 spec」指针，收尾再改终态）

**Interfaces:**
- Produces: 完整 dll_stem / Export 宏对照（与 spec §2 表一致）；扫描脚本退出码非 0 表示仍有扁平 include 或 `Export_Smt` / `dll_stem = "Smt`

- [ ] **Step 1:** 把 spec 中 dll_stem 表与附录 A include 模式抄入 `abi-rename-map.md`，并列出已知同名头消歧（扫 `src/**/*.h` 基名冲突）。
- [ ] **Step 2:** 写扫描脚本：在 `src/`、`testing/` 找 `#include "foo.h"`（无斜杠）、`Export_Smt`、`dll_stem = "Smt`。
- [ ] **Step 3:** 提交映射表 + 脚本（允许此时扫描失败，作为基线）。

---

### Task 2: GN include 根收口（可与 3–8 并行定稿，但合并顺序：改码后最后删 dirs）

**Files:**
- Modify: `build/BUILD.gn` — `config("legacy")` 的 `include_dirs`
- Modify: `build/BUILDCONFIG.gn` — 确认 `include_dirs` 含 `//src`（或 `//`+约定；本仓锁定 `//src`）
- Modify: 各 `src/**/BUILD.gn` — `dll_stem` 与导出 `defines`

**Interfaces:**
- Consumes: `abi-rename-map.md` stem/宏名
- Produces: 无模块级产品 `include_dirs`；新 `output_name` 来自新 stem

- [ ] **Step 1:** 按映射表改所有 `dll_stem = "Smt…"`。
- [ ] **Step 2:** 全局替换各目标里 `Export_Smt*` define 为新 `*_EXPORT` 定义宏。
- [ ] **Step 3:** **在 Tasks 3–8 源码改完后**，删除 `config("legacy")` 中全部 `$smt_src/...` 模块 `include_dirs`；保留 MBCS / forced include / 必要 defines。
- [ ] **Step 4:** 勿在本任务单独要求 ninja 绿（大爆炸）。

---

### Task 3: `base` + `sys` 切断

**Files:**
- Modify: `src/base/**`, `src/sys/**`（所有 `.h`/`.cpp`/`.cc`）
- Modify: 引用这些头的调用方若落在本分区外，由对应 Task 改；本 Task 负责树内自洽

**Interfaces:**
- Produces: `"base/core/…"`, `"sdb/carto/…"`, `"base/ipc/…"`, `"sys/…"`；`CORE_EXPORT` / `STYLE_EXPORT`；命名空间 `base`（+ `detail`）

- [ ] **Step 1:** 树内所有 `#include` 改为附录 A 路径。
- [ ] **Step 2:** 导出宏、命名空间、公开函数 snake_case（按映射表；ABI 全破）。
- [ ] **Step 3:** 更新树内自测若有。

---

### Task 4: `algorithm` 切断

**Files:**
- Modify: `src/algorithm/**`

**Interfaces:**
- Consumes: `base`/`sys` 新路径与符号
- Produces: `"algorithm/geo/…"`, `geo` / `proj` / `tin` / `stat` 命名与 `GEO_EXPORT` 等

- [ ] **Step 1–3:** 同 Task 3 模式（include → 宏/命名空间/snake_case → 局部测试）。

---

### Task 5: `sdb` 切断

**Files:**
- Modify: `src/sdb/**`

**Interfaces:**
- Consumes: `base`、`algorithm`、GDAL 三方头
- Produces: `"sdb/…"`, `GIS_EXPORT` / `SDE_*_EXPORT`, 命名空间 `sdb`

- [ ] **Step 1–3:** 同 Task 3；datasource 子树路径必须含 `sdb/datasource/<driver>/`。

---

### Task 6: `render` + `gpu` 切断

**Files:**
- Modify: `src/render/**`, `src/gpu/**`

**Interfaces:**
- Consumes: `sdb`、`algorithm`、`base`
- Produces: `"render/…"`, `"gpu/…"`, 对应 `RENDER_*_EXPORT`

- [ ] **Step 1–3:** 同 Task 3。

---

### Task 7: `net` + `tool` 切断

**Files:**
- Modify: `src/net/**`, `src/tool/**`

**Interfaces:**
- Produces: `"net/…"`, `"tool/…"`；`NET_EXPORT` / `TOOL_EXPORT`；`tool::` / `net::`

- [ ] **Step 1–3:** 同 Task 3。新树已是路径 include 的文件仍须去掉任何残留扁平引用并统一命名。

---

### Task 8: `ui` + `plugin` + `app` + `content` + `testing`

**Files:**
- Modify: `src/ui/**`, `src/plugin/**`, `src/app/**`, `src/content/**`, `testing/**`
- Modify: `src/plugin/legacy_am.cc`（及同类 stem 表）

**Interfaces:**
- Consumes: 全部下层新 ABI
- Produces: 插件 id→新 dll 文件名；MFC 应用类去 `Smt`；content/ui/views 路径 include 一致

- [ ] **Step 1:** Include + 导出 + 命名空间/snake_case。
- [ ] **Step 2:** 重写 `legacy_am` stem 映射；删对 `SmtAM*` 的依赖。
- [ ] **Step 3:** 测试树跟随符号与 include。

---

### Task 9: 集成、验收、文档终态

**Files:**
- Modify: `build/BUILD.gn`（若 Task 2 Step 3 未做完）
- Modify: `docs/build/src-layout.md`, `docs/build/mogu-mapping.md`, `src/README.md`, 根 `README.md`
- Run: `tools/cutover/check_flat_includes.py`（或等价）

**Interfaces:**
- Consumes: Tasks 2–8 全部落地
- Produces: 验收清单全绿

- [ ] **Step 1:** 跑扫描脚本 — 期望无残留扁平 include / `Export_Smt` / `Smt` dll_stem。
- [ ] **Step 2:** `build.bat`（`src_all`）修到绿。
- [ ] **Step 3:** `build.bat app`、`build.bat views` 修到绿。
- [ ] **Step 4:** 计划内测试 / 插件冒烟绿。
- [ ] **Step 5:** 更新 as-built 文档与根 README「最后更新」；spec 保持 `accepted` 直至全部落地后可标 `landed` 并归档（另一次变更亦可）。
- [ ] **Step 6:** 最终 commit（信息说明破 ABI / include 切断，无 Cursor co-author）。

---

## 并行说明

| 可并行 | 注意 |
| --- | --- |
| Tasks 3–8 | 目录不相交；跨树 `#include` 两侧都改到新路径 |
| Task 1 | 最先完成 |
| Task 2 Step 1–2 | 可与 3–8 早并行 |
| Task 2 Step 3 + Task 9 | 必须最后 |

## 风险与回滚

- `master` 红窗口长：用映射表 + 扫描脚本减漏改；不要用恢复 `include_dirs` 掩盖。
- 回滚：整段变更集 `git revert`（大爆炸无分层绿可部分回滚）。

---

**最后更新：** 2026-09-13
