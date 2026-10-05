<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: RHI suite + bench + Console `:rhi` (coverage Phase 1)

> **For agentic workers:** Implement task-by-task. Checkboxes track progress.

**Goal:** Industry-shaped Null RHI functional suite + QPC benches in `benchmark_all`, optional DX12 GPU bench, Debug Console `:rhi test|bench` spawn — per living RHI umbrella §RHI suite / coverage / bench.

**Architecture:** Shared `src/render/testing` scenarios → `rhi_suite_test` / `rhi_bench` / `rhi_gpu_bench`; Console mirrors `:gis` harness spawn.

**Tech Stack:** C++23, GN `benchmark()` → `//third_party:gbenchmark` (+ `gbenchmark_main` by default); install via `build.bat t benchmark`.

## Global Constraints

- Work on `master`; no new dated design twin.
- Default CI Null only; GPU via `RUN_FLYCUBE_GPU=1` / `rhi_gpu_bench` not in default `benchmark_all`.
- No FlyCube types in public headers; no Qt.
- snake_case; English comments; copyright 2026.

## Tasks

- [x] Living § on `2026-09-13-render-rhi-scene-design.md`
- [x] `src/render/testing` scenarios + suite + benches + GN
- [x] Wire `test_shell` / `benchmark_all`
- [x] DebugAgent `:rhi test|bench` + help + coverage expect
- [x] Switch all repo `benchmark()` targets to `//third_party:gbenchmark`
- [ ] Phase 2: OpenCppCoverage / MSVC coverage script (deferred)

## Verify

```bat
.\build.bat debug rhi_suite_test
.\out\Debug\rhi_suite_test.exe
.\build.bat debug rhi_bench
.\out\Debug\rhi_bench.exe
.\build.bat debug b
```
