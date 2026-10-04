<!--

Copyright (c) 2026 The Mogu Authors.

All rights reserved.

-->



# Base execution Implementation Plan



> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.



**Goal:** Land mogu-aligned `src/base/execution` (+ sync/concurrency prerequisites) and wire first-wave call sites for CPU parallel + async speedups.



**Architecture:** Hybrid port: API/dirs match mogu; Windows uses `std::mutex` and a portable `IOExecutor`; Linux-only backends gated. Foundation GN aggregate pulls new modules.



**Tech Stack:** C++23, GN/Ninja (`build.bat`), moodycamel concurrentqueue, gtest.



## Global Constraints



- Stay on `master`; no `base::mutex`; comments in English; snake_case new functions; two-level public namespaces (`base::execution`).

- Output only under `out/`; include paths `#include "base/..."`.

- Spec: `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`.



---



### Task 1: Vendor concurrentqueue



- [x] Add `third_party/.src/concurrentqueue` (copy from mogu or fetch) + `third_party/concurrentqueue/BUILD.gn` with include layout `concurrentqueue/moodycamel/*.h`

- [x] Register `//third_party:concurrentqueue` forwarder

- [ ] Optional manifest entry with `install_skip: true`



### Task 2: synchronization + concurrency + memory extras



- [x] Port `src/base/synchronization/` (all headers except `mutex.h`)

- [x] Port `src/base/concurrency/` (queue + peers needed by execution)

- [x] Add `singleton.h`; port or stub `allocator.h` / `arena.h` if pipeline/pools require

- [x] Minimal `base/util/type_utils.h` (no demangle hard dep) and `base/tuple/` for futures `when_all`

- [x] `container/iterator.h` if map_reduce needs it; drop unused `archiver.h` include from map_reduce IO if unused

- [x] GN source_sets + foundation public_deps

- [x] Unit smoke: latch + BlockingQueue push/pop



### Task 3: execution tree



- [x] Copy `src/base/execution/` mogu shape

- [x] Rewrite portable `IOExecutor` (no `base/io` / epoll)

- [x] Gate `io_uring` / device backends behind `OS_LINUX` or stub headers

- [x] Remove unused allocator includes or provide stubs so MSVC builds

- [x] `//src/base/execution:execution` + `execution_test`

- [x] Wire into `:foundation`



### Task 4: call-site adoption



- [x] `plugin::ProcessingPool` → executor / global pool

- [x] At least one GIS CPU path (`tessellate_*` or field ingest) → `parallel_for`

- [x] One Pipeline or futures `async`/`then` adoption where natural (`visible_layer_batches` Pipeline; `rebuild_layout` async batches; OGR feature load Pipeline)

- [x] Trace-driven layout: cross-layer fill/line `parallel_for`, MultiLineString flatten, build on caller



### Task 5: verify



- [x] `build.bat` green for foundation + new tests

- [x] `build.bat te` or targeted `*_test` for execution

- [x] Update `src/base/README.md`, `docs/superpowers/mogu-mapping.md`, Active table in `docs/superpowers/README.md`



---



**最后更新：** 2026-09-28

