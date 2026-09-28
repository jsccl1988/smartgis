<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base` Hybrid 全量替换（原「仓库根 `base/`」）

**Date:** 2026-09-14  
**Status:** accepted（Phases 0–6 consolidator 已收口；**2026-09-15 amendment：** foundation 真源从仓库根迁入 `src/base/`；**2026-09-28：** §Trace / §Memory；**2026-09-28：** §Process malloc / PA-E）  
**Updated:** 2026-09-28 — §Carto split (`Envelope` → `gis`；style POD → `legacy/carto`)。merge B: compressed child specs into this living umbrella (see §Folded topics). Do not open new dated twins.
**Goal:** 将遗留 `src/base/core`（`Smt*`）对照 mogu **全部**替换到 foundation 树；制图 style / `sys` / `net` 留在产品层；分期 strangler，阶段末不留旧名转发壳。  
**Related:** [`../../build/src-layout.md`](../../build/src-layout.md)、[`../../build/mogu-mapping.md`](../../build/mogu-mapping.md)、[`../../build/abi-rename-map.md`](../../build/abi-rename-map.md)、[`2026-09-13-code-style-include-abi-cutover-design.md`](../archive/specs/2026-09-13-code-style-include-abi-cutover-design.md)、[`2026-09-13-base-archive-design.md`](../archive/specs/2026-09-13-base-archive-design.md)、[`2026-09-13-base-ipc-mojom-design.md`](../archive/specs/2026-09-13-base-ipc-mojom-design.md)、[`2026-09-14-dll-reorganization-design.md`](../archive/specs/2026-09-14-dll-reorganization-design.md)  
**Plan (Cursor):** `base_root_hybrid_fd0c40fd.plan.md`（会话外；本仓以本 spec + `docs/build` 为准）；**§Memory:** [`../plans/2026-09-28-base-memory.md`](../plans/2026-09-28-base-memory.md)；**§PA-E:** [`../plans/2026-09-28-partition-alloc-everywhere.md`](../plans/2026-09-28-partition-alloc-everywhere.md)

## Amendment (2026-09-15)

| 项 | 原 Phase 0–6 约定 | 现行 |
| --- | --- | --- |
| 真源落点 | 仓库根 **`base/`** | **`src/base/`**（与产品平台 DLL 同树；foundation 在 `base/core`，Smt leftovers 在 `legacy/core`） |
| 首选 GN | `//base:base` | **`//src/base:foundation`** |
| 仓库根 `base/`、`core/` | 真源树 / 别名目录 | **已删除**；兼容别名仅在根 `BUILD.gn`（`//:base`、`//:core` → `:foundation`；`//:core_all` → `//src:src_all`） |
| Include | `BUILDCONFIG` 的 `//` 命中根 `base/` | **`//src` 优先** → `#include "base/..."` 仅命中 `src/base/` |

历史 Phase 叙述仍保留「根 `base/`」措辞，表示当时交付；以本节与 Locked decisions 现行表为准。

## Motivation

- 产品树已分层，但基础面曾是 2010 `Smt*`（`SmtLogManager` / `SmtCSLock` / `SmtDynLib` …），与 mogu foundation 不对齐。
- Hybrid 分期已把 mogu 式头面落到 foundation；产品约定是 **产品代码在 `src/`**，故 foundation 真源亦迁入 `src/base/`，仓库根不再放源码树。
- 「一层一 DLL」的产品 stem 与 foundation GN 标签曾易混；**磁盘 stem = `base`**（与 `src/base` 同名），foundation = `:foundation`（无 DLL）。

## Locked decisions（现行）

| Topic | Choice |
| --- | --- |
| 范围 | 整棵 `src/base/core` 对照 mogu 完成；不是单模块试点 |
| 手法 | **Hybrid**：薄面 port mogu header；厚/平台面按 mogu API 形状本仓 rewrite（Windows：`LoadLibrary`；无裸拷 `unistd`/`dlfcn`） |
| 落点 | 终局 foundation = **`src/base/`**；仓库根**无**物理 `base/`、`core/`；兼容别名 `//:base` / `//:core` |
| 边界 A — 进 foundation | `core`（headers）、`threading`、`files`、`memory`、`util`、`archive`、`ipc`、`synchronization` / `concurrency` / `execution`（见 [`2026-09-28-base-execution-design.md`](../archive/specs/2026-09-28-base-execution-design.md)）；按需 `string` / `time` / `traits` / `container` / `tuple` 子集 |
| 边界 A — 留产品层 | 制图 pen/brush/`SmtStyle` → **`src/legacy/carto`**（仍链入 `base.dll`）；`gis::Envelope` → **`src/gis/model/envelope.h`**（header-only）；`sys`、`net` 不动 |
| 硬排除 | 不搬 mogu `base::mutex`（新树 `std::mutex`）；不整棵搬 archive Json/Text/Yaml sink；不 vendor Chromium；不引入 Qt |
| ABI | 破 `Smt*`；**阶段末不留**旧名转发壳；日常改动在 `master` |
| 推进 | **分期 strangler**（每期绿再进下一期） |
| 链接 | `//src/base:foundation` = `source_set` 聚合（多为 header-only） |
| 产品 DLL stem | **`dll_stem=base`** |

## DLL / 标签命名

| 名称 | 含义 | 磁盘 |
| --- | --- | --- |
| **`//src/base:foundation`** | mogu 式 foundation | **无**产品 DLL；静态链入消费方 |
| **`//:base`** / **`//:core`** | 根 `BUILD.gn` 转发 → `:foundation` | 无物理 `base/`、`core/` 目录 |
| **`//src/base:base`**（+ legacy alias `:platform`） | 产品平台层 shared_library | **`dll_stem=base`** → `base.dll` / `base_d.dll` |
| **产品平台 DLL 内容** | `legacy/core` leftovers + `legacy/carto` + xml + `sys`（net 已独立） | style POD 链入本 DLL；`gis::Envelope` 不在本 DLL |

规则：

1. 文档与代码评论中区分 **GN 标签 `foundation` / 兼容 `//:base`** 与 **磁盘 `dll_stem=base`**。
2. **`dll_stem=base` 已落地**；`#pragma comment(lib, "base…")` 与顶层目录同名，不另开 `platform` stem。
3. DLL reorg「一层一 DLL」对 **产品层** 仍有效。

## Include 根

| 根 | 用途 | 示例 |
| --- | --- | --- |
| `//src`（`BUILDCONFIG` 优先 + `//build:legacy`） | foundation + 产品树 | `#include "base/core/log.h"`；`#include "sdb/map/map.h"` |
| `//` | 仓库根辅助 | build helpers；**不再**承载 foundation 源码 |

## 目标布局

```text
src/base/                 # //src/base:foundation + //src/base:base (base.dll)
  core/                   # foundation headers only (log/macros/export/debug/build_config)
  threading/ util/ files/ memory/ time/
  archive/                # //src/base/archive:archive
  ipc/                    # //src/base/ipc:ipc
# (no repo-root base/ or core/ — //:base and //:core forward in BUILD.gn)
src/
  legacy/core/            # Smt leftovers → core_sources → base.dll
  legacy/carto/  legacy/{xml,sys}/  net/  plugin/
```

## 遗留 → 终局（摘要）

| 遗留 | 终局 | 手法 |
| --- | --- | --- |
| `log` / `logmanager` | `base/core/log.h` | Port + Win 适配 |
| `core_assert` / 宏面 | `base/core/{macros,debug,export,build_config}.h` | Port |
| `cslock` / `srwlock` / `trd_sync` | `std::mutex` / `shared_mutex` | Rewrite；**不** port `synchronization/mutex.h` |
| `thread` / `threadpool` / `workthread` | `base/threading` | Port 薄 API + rewrite |
| `filesys` | `base/files` + `base/util/path` | Hybrid |
| `dynlib*` | `base/util/library` | Port + `LoadLibraryW` |
| `plugin*`（core 内） | `base/util/plugin` + `src/plugin` | Hybrid |
| `mempool*` | `base/memory` 子集（`object_pool`） | Hybrid |
| `memshare` | **deleted**（无调用方；产品路径不需要） | Drop |
| `timer` | `base/time` | Port/rewrite |
| `listener` / `command` / `msg*` | 不进 foundation；已迁 `src/legacy/core`（仍编入 base.dll） | Rewrite / 删除 |
| `xml*` | `src/legacy/xml` | 平移 |
| `winservice` | **deleted**（无调用方；非 Views 产品路径） | Drop |
| `matrix2d.h` | `algorithm` 或 `sdb` | 平移 |
| `src/base/style` | `src/sdb/carto` | 搬家 |
| archive / ipc | `src/base/archive`、`src/base/ipc` | 曾上移根 `base/`，现与 foundation 同回 `src/base` |

## Phases（历史；均已收口）

| Phase | 内容 | 绿门槛 |
| --- | --- | --- |
| **0** | 根 `base/` GN+README、include、DLL 命名文档、本 spec、`//core`→foundation | 当时 `//base:base` 可解析 |
| 1–4 | core / threading / path / memory+time | 相关单测 / `src_all` |
| 5 | archive/ipc 曾上移根 `base/` | 后经 amendment 迁入 `src/base` |
| 6 | carto、xml、`dll_stem=platform`（后改为 `base`）、deferred leftovers | `build.bat` / `src_all` |
| **amendment** | 真源 `base/` → `src/base/`；`:foundation`；根物理 `base/` 与 `core/` **删除**；`//:base` / `//:core` 转发 | foundation + `base_d.dll` + `ipc_test` |

## Leftover core（已迁出 `src/base/core`）

`listener` / `command` / `msg*` / `api` / `bas_struct` / `env_struct` / `core_assert` / `core.h` / `core_exception` 已迁至 `src/legacy/core`（`#include "legacy/core/…"`；`core_sources` → base.dll）。后续仍可随调用方重写删除或再拆到 `src/tool` / `src/plugin`。`SmtSysManager` 在 `src/legacy/sys`（仍编入 base.dll）。

## 风险

- mogu `log.h` / `library.h` 含 POSIX；必须 Windows 适配。
- 勿把 `//src/base:foundation`（或兼容 `//:base`）与产品 `dll_stem=base` 混称。

## §Trace（2026-09-28）

**Status:** active  
**Plan:** [`../plans/2026-09-28-render-trace-profiler.md`](../plans/2026-09-28-render-trace-profiler.md)

mogu-aligned `src/base/trace/` in foundation (header-mostly):

| API | Header | Notes |
| --- | --- | --- |
| `base::Trace` / `ScopedTracer` | `base/trace/trace.h` | Mutex + deque ring (no mogu `NonblockingQueue`) |
| `process_trace` / `BASE_TRACE_EVENT` / `SMT_TRACE` | `base/trace/process_trace.h` | Process-wide; default off |
| `SpanRecorder` | `base/trace/span_recorder.h` | Preallocated slots + overflow (P3) |
| `export_chrome_trace` | `base/trace/chrome_trace.h` | `{"traceEvents":[...]}` |

Dump shape locked: Chrome Trace Event Format object (Perfetto / `chrome://tracing`). Categories for present: `map2d.*`, `scene3d.*`, `viewport.frame`.

GN: `//src/base/trace:trace` public_dep of `:foundation`. Test: `trace_test`.

## §Memory（2026-09-28）

**Status:** active  
**Plan:** [`../plans/2026-09-28-base-memory.md`](../plans/2026-09-28-base-memory.md) (Batch1–2 landed); Batch3a [`../plans/2026-09-28-gis-memory-load.md`](../plans/2026-09-28-gis-memory-load.md)

mogu-aligned `src/base/memory/` as `//src/base/memory:memory` (public_dep of `:foundation`):

| API | Header | Notes |
| --- | --- | --- |
| `MemoryResource` + PMR backends | `memory_resource.h` | `kNewDelete` / `kMonotonicBuffer` / `kUnsynchronizedPool` / `kSynchronizedPool` / `kHybridOptimized`. **No `kProtobuf`.** |
| `Arena` / `allocate` / `tls_*` | `arena.h` | Process singleton + TLS hybrid |
| `ObjectAllocator` / `create` / `STLAllocator` | `allocator.h` | Delayed destroy via `PushOnlyQueue` |
| `ObjectPool` | `object_pool.h` | Free-list → `shared_ptr` recycle |
| `singleton` / `scope_guard` / `allocation_tracker` | matching headers | RAII / diagnostics |

**Thin deps (already under foundation):** `base/concurrency/queue.h` (moodycamel `//third_party:concurrentqueue`), `base/synchronization/align.h`, `base/traits/{is_detected,concept,class_traits}.h`. Still **no** `base::mutex`.

**Lifetime rule (scratch vs durable):** temporary decode / pipeline scratch may use TLS hybrid or monotonic Arena; long-lived `MapFeature` / layer store / `OGRGeometry::clone()` stay on the default (or GDAL) heap.

**Adoption batches**

1. Present / frame — `Layout::build` + `Map2dFrameCache` + `effect::map::Pass` clear TLS / monotonic scratch per build/record. **Landed.**
2. GIS model — tileset JSON parse (`ObjectPool` keys + monotonic Arena); OGR decode TLS scratch. **Landed.**
3. GIS OGR load (Batch3a) — `load_ogr_layer_pipeline` ordered window (bound in-flight `Out`) + decode TLS clear cadence + **Ctx freelist** via `Pipeline::set_context_hooks`. Plan: [`../plans/2026-09-28-gis-memory-load.md`](../plans/2026-09-28-gis-memory-load.md).
3b. Vista tessellate scratch (Batch3c) — TLS clear on public `tessellate_*` entry; thread_local `ObjectPool` for PolyPt/Vec2/dash scratch vectors (parallel_for safe).
3c. Durable `MapFeature` / `gis::Feature` — **not** ObjectPool’d (move into `LayerStore` would strip pooled capacity); long-lived ownership stays default heap.
4. Diagnostic Tools **Memory** tab — `sample_memory_counters_to_process_trace()` + stats strip (`AllocationTracker` optional).

Test: `memory_test`; Batch3a: `feature_load_pipeline_test` + `execution_test`; Batch3c: `tessellate_style_test`.

## §Process malloc / PartitionAlloc-Everywhere（2026-09-28）

**Status:** active（Phase 0 done；Phase 1+ checklist in [`../plans/2026-09-28-partition-alloc-everywhere.md`](../plans/2026-09-28-partition-alloc-everywhere.md)；**no allocator implementation until Phase 2/3 gates**）  
**Why not a new dated spec:** considered folding into §Memory only; rejected — §Memory is mogu PMR / `ObjectAllocator` (explicit pools). Process-heap replacement is Chromium PA-E + shim, a separate contract. Living home stays this file (Active row already owns `src/base` memory).

### Chromium model (normative)

```
malloc / operator new  →  allocator_shim  →  PartitionAlloc (PA-E)
```

| Flag / constraint | Chromium fact |
| --- | --- |
| `use_partition_alloc` | PA library available |
| `use_allocator_shim` | Hijack CRT / `malloc` / `new` |
| `use_partition_alloc_as_malloc` | Shim default dispatch → PA (PA-E) |
| tcmalloc | **Removed** from Chromium; **not supported on Windows** |
| Windows + Debug CRT | Shim / PA-E **off** (incompatible with debug heap) |
| Windows + component / multi-DLL | Historically **off**; modern path requires a shared **`allocator_shim` DLL** linked by **every** DLL and exe — otherwise mixed heaps crash |

This repo’s product graph (`base.dll`, `gis.dll`, `render.dll`, …) is **component-like**. Enabling any third-party global allocator without Chromium’s single-shim rule reopens pitfalls Chromium already documented.

### Locked decisions

| Topic | Choice |
| --- | --- |
| Process heap endgame | **PartitionAlloc-Everywhere** via Chromium **allocator_shim** only |
| Rejected | gperftools / tcmalloc, mimalloc, jemalloc as process malloc |
| Relation to §Memory | Orthogonal: PMR / `ObjectAllocator` remain explicit arenas; they do **not** replace process `malloc` |
| Default until gates | `use_partition_alloc_as_malloc = false` (system CRT); **no** global override in tree |
| Misleading leftovers | `USE_TCMALLOC` in `build/build_config.h` and unused `//build/config/posix:gperftools` are **deprecated signals** — must not be treated as “tcmalloc enabled”; clean up in Phase 4 |

### Hard gates (copy Chromium; do not invent)

All must hold before product PA-E may default on:

1. **Single heap owner:** either a near-Chrome **non-component** link of the browser/Views process, **or** a Chromium-style shared **`allocator_shim`** that **every** product `shared_library` and `executable` depends on (missing one module ⇒ mixed heap).
2. **Windows Debug:** PA-E **off** by default (same as Chromium).
3. **Sanitizers:** ASan / LSan / TSan / MSan ⇒ shim **off**.
4. **No interim global malloc:** do not ship mimalloc/jemalloc/tcmalloc “while waiting for PA”.

### Phases

| Phase | Work | Deliverable |
| --- | --- | --- |
| **0** | This § | Design only — **no code** |
| **1** | Pin Chromium `partition_alloc` (+ Windows shim); mirror GN args `use_allocator_shim` / `use_partition_alloc_as_malloc`; dependency graph | Spec + graph; flags still default **off** |
| **2** | Prove PA-E on a **single executable** test target (no product DLL graph) | Verified static path |
| **3** | Product multi-DLL: shared `allocator_shim` + default `shared_library` / `executable` deps | Only then allow product default on |
| **4** | As-built in `docs/build/`; remove or neutralize `USE_TCMALLOC` / dead gperftools config | Closeout |

### Explicit non-goals (until Phase 3+)

- Enabling Linux system `libtcmalloc*` via posix `gperftools` config for `src` binaries.
- Linking mimalloc / jemalloc into exe or `base.dll`.
- Turning on any process-heap override under the current Debug + multi-DLL defaults.

## 与既有 spec 的关系

- **修订** src-layout：foundation 真源仅在 `src/base/`；仓库根无物理 `base/`；产品平台 DLL **stem = `base`**（与顶层目录同名）。
- **沿用** A1 archive、IPC named-pipe、mutex 规则、DLL「一层一 DLL」对产品层。
- **§Process malloc：** 进程堆对齐 Chromium PA-E；与 §Memory（mogu 显式池）分层；未过 Phase 2/3 门槛前不落地分配器实现。

---

## §Carto split（2026-09-28）

**Why not a new dated spec:** layout / include break owned by as-built `docs/build/src-layout.md` + this living base row (ban list: subdirectory moves).

| Piece | Path | DLL / linkage |
| --- | --- | --- |
| `gis::Envelope` | `src/gis/model/envelope.h`（header-only） | no export from `base.dll`；产品 `gis` / leftover 共用 |
| `SmtStyle` / StyleManager / `style_api` | `src/legacy/carto/` | `carto_sources` → **`base.dll`**（`to_smt_style` 桥仍在产品 `gis`） |

`src/base/carto` removed. Do not confuse with GDI `legacy/render/.../gdi/carto` (`MapCarto2d`).

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-base-archive-design.md`](../archive/specs/2026-09-13-base-archive-design.md) | §archive (folded) |
| [`../archive/specs/2026-09-13-base-ipc-mojom-design.md`](../archive/specs/2026-09-13-base-ipc-mojom-design.md) | §ipc / mojom (folded) |
| [`../archive/specs/2026-09-28-base-execution-design.md`](../archive/specs/2026-09-28-base-execution-design.md) | §execution / sync / concurrency (folded) |
| [`../archive/specs/2026-09-28-third-party-json-xml-protobuf-design.md`](../archive/specs/2026-09-28-third-party-json-xml-protobuf-design.md) | §third-party JSON/XML/protobuf (folded) |

