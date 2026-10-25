<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/network`

Shared directed line-network kernels (`LineNetwork`) plus solvers.

| Id | Role |
| --- | --- |
| `native.cost_path` | Dijkstra / bidirectional / A* least-cost path (optional oneway, turns, barriers) |
| `native.service_area` | Cost-cutoff reachable edges |
| `native.apsp` | Multi-source cost matrix (APSP subset) |
| `native.max_flow` | Edmonds–Karp max flow |

Build options: `weight_field`, `capacity_field`, `oneway_field`, `turns_path`, `barriers_path`.  
See living **§10** in `docs/superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`.
