<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS Coverage + Performance Benchmark Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Full-tree `src/gis` functional + compiler coverage and QPC performance baselines (dual vs OGR/GEOS/PROJ on critical paths), reachable from CLI and Debug Console.

**Architecture:** Aggregate GN groups `gis_test_all` / `gis_benchmark_all`; colocated `*_test` / `*_benchmark`; coverage script under `testing/coverage/`; Console `:gis test|bench` spawns the same exes. Spec: [`../specs/2026-09-13-gdal-layer-management-design.md`](../specs/2026-09-13-gdal-layer-management-design.md) §GIS coverage + performance benchmarks.

**Tech Stack:** C++23, GN/Ninja, QPC micro-bench, OpenCppCoverage (optional), `//third_party:gdal`.

## Global Constraints

- Work on `master` only; partition paths across parallel agents.
- No Qt; Views + Skia for UI; Console is spawn-only for GIS harness.
- New-tree functions `snake_case`; namespaces ≤ two public layers.
- Copyright year **2026**; English comments.
- Gen roots `out/Debug` / `out/Release`; prefer `build.bat debug …`.
- Do **not** open a new dated design twin — revise the GDAL living umbrella § only.
- Match peer bench style (`views_bench` / `content_console_bench`), not a mandatory google_benchmark migration in v1.

## File map

| Path | Role |
| --- | --- |
| `src/gis/BUILD.gn` | `gis_test_all` / `gis_benchmark_all` |
| Root `BUILD.gn` | Wire groups; ensure missing tests listed |
| `src/gis/geo/ops/buffer_benchmark.cc` | buffer dual |
| `src/gis/geo/proj/proj_benchmark.cc` | transform dual |
| `src/gis/datasource/session/datasource_benchmark.cc` | open+iterate |
| `testing/coverage/gis_coverage.ps1` | coverage / functional summary |
| `docs/superpowers/gis-test-matrix.md` | capability matrix |
| `src/content/browser/debug/debug_agent.cc` | `:gis test\|bench` |

---

### Task 1: Living § + matrix + plan (docs)

- [x] Append § to GDAL umbrella
- [x] Write this plan
- [x] Land `docs/superpowers/gis-test-matrix.md` + index links
- [x] Refresh `docs/superpowers/README.md` Active plan cell

### Task 2: GN aggregates + missing tests

- [x] `//src/gis:gis_test_all` lists every `src/gis/**` `test()` target
- [x] `//src/gis:gis_benchmark_all` lists GIS benches
- [x] Root `test_shell` / `benchmark_all` depend on those groups (dedupe inline deps where practical)
- [x] Add any missing tests currently defined but not in `test_shell` (`feature_test`, `edit_conflict_test`, `tileset_test`, atmosphere cloud/ocean/environment)

### Task 3: Kernel + datasource benches

- [x] `buffer_benchmark` (ours buffer vs OGR Buffer)
- [x] `proj_benchmark` (4326→3857 dual)
- [x] `datasource_benchmark` (mem create + iterate)
- [x] `build.bat debug b` includes them

### Task 4: Coverage runner

- [x] `testing/coverage/gis_coverage.ps1`
- [x] Document in `testing/README.md`

### Task 5: Console bridge

- [x] `:gis test` / `:gis bench` in DebugAgent
- [x] Update `:help` + coverage test expectations

### Task 6: Verify

- [x] `ninja -C out/Debug gis_test_all` (or `build.bat debug` subset)
- [x] Run one bench exe; run coverage script smoke
