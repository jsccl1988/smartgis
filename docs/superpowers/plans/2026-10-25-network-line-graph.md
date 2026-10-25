<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Network LineGraph 三阶段 Implementation Plan

> **For agentic workers:** 按任务勾选；内核在 `src/gis/analysis/network`，公开面 `gis::detail` + `native.*`。

**Goal:** 共享有向 `LineNetwork`（转向/障碍）并对标库侧算法：双向 Dijkstra/A*、服务区、APSP 子集、最大流；迁移既有 `native.cost_path`。

**Architecture:** Approach A — `line_network` 建图一次；求解器只吃内存图；`OpsRunner` 域表注册新 `native.*`；不 vendor OSRM/Boost.Graph。

**Tech Stack:** C++23、GDAL/OGR、Eigen（长度范数）、gtest、`plugin::run_builtin_op`。

**Spec:** [`../specs/2026-09-13-algorithm-layer-oss-design.md`](../specs/2026-09-13-algorithm-layer-oss-design.md) §10  
**Diagram:** [`../diagrams/gis-analysis-network.html`](../diagrams/gis-analysis-network.html)

## Global Constraints

- 公共命名空间两层；实现落 `gis::detail`；函数 `snake_case`。
- 默认无 `oneway_field` → 双向边（保持 traffic gate / 靠右折中）。
- 不 vendor 完整路由引擎；不做 VRP / 复杂交叉口几何。
- 产品↛leftover；不改 `content/public`。

---

## Tasks

### Task 1 — `LineNetwork` + 迁移 `cost_path`

- [x] `network/line_network.{h,cc}`：吸附结点、有向/`oneway`、代价/容量、转向禁行、点/线障碍
- [x] `network/shortest_path.{h,cc}`：Dijkstra（含靠右 tie-break）
- [x] `cost_path` 改为建图 + Dijkstra；API 签名兼容
- [x] `analysis_network_test`：无向 L 形、靠右、单向、转向禁行、障碍
- [ ] `build.bat debug analysis_network_test` 绿；traffic gate 仍绿（受 out/Debug 并行争用阻塞时复跑）

### Task 2 — 双向 Dijkstra / A* + 服务区

- [x] `shortest_path`：`bidirectional` / `astar`；`native.cost_path` 可选 `algorithm`
- [x] `service_area.{h,cc}` + `native.service_area`（cutoff → MultiLineString）
- [x] 测试：等权网格双向=单源；服务区半径覆盖

### Task 3 — APSP 子集 + 最大流

- [x] `apsp.{h,cc}`：多源 Dijkstra（非全 Floyd）+ `native.apsp`
- [x] `max_flow.{h,cc}`：Edmonds–Karp + `native.max_flow`
- [x] 测试：三源代价矩阵；小容量割

### Task 4 — 文档收口

- [x] Living §10、本计划勾选、`network/README.md`、diagram
- [x] Active 表 Plans 行挂上本计划
