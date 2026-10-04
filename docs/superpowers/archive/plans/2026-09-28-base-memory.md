<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# base/memory Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** Landed in the same change as the §Memory design amendment. Checkboxes track verification.

**Goal:** mogu-aligned `src/base/memory` (minus `kProtobuf`) + Batch1 present/frame and Batch2 GIS model hot-path adoption.

**Architecture:** `//src/base/memory:memory` source_set (like `trace`); foundation `public_deps` it. Queue/align/traits already in foundation tree; pin `concurrentqueue` for `PushOnlyQueue`.

**Tech Stack:** C++23, `std::pmr`, moodycamel concurrentqueue, GN/`build.bat`.

## Global Constraints

- No `base::mutex` / no `kProtobuf` / no Qt / work on `master` / `out/` only.
- Living doc: amend `2026-09-14-base-root-hybrid-design.md` §Memory (no new dated design).

## Tasks

- [x] Pin `concurrentqueue` (`manifest` + `third_party/concurrentqueue` include layout + GN)
- [x] Port memory headers; strip protobuf; `memory_test`
- [x] Wire `:foundation` → `:memory`
- [x] Batch1: layout / Map2dFrameCache / Pass scratch
- [x] Batch2: tileset ObjectPool + Arena; OGR decode TLS clear
- [x] `build.bat memory_test` (+ tileset_test ok; frame/map pass failures look pre-existing visual asserts)

**Follow-up:** Batch3a GIS OGR load path → [`2026-09-28-gis-memory-load.md`](../../plans/2026-09-28-gis-memory-load.md).
