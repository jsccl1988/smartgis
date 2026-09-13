<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Base archive（A1）实现计划

> **For agentic workers:** Stay on `master`。Do **not** commit unless the user asks.

**Goal:** 将 BinarySink / Serializer / archive helpers 从 `src/net/pack/archive.h`（`net`）上提到 `src/base/archive/archive.h`（`base`）；`net::Pickle` 留在 `src/net/pack/pickle.h`；`base/ipc` codec 依赖 `base/archive`，不依赖 net archive。

**Status:** done（2026-09-13）

**Spec:** [`../specs/2026-09-13-base-archive-design.md`](../specs/2026-09-13-base-archive-design.md)

## Global Constraints

- Stay on `master`；不开 topic 分支。
- 文档中文；源码注释英文。
- Wire / 帧格式不变；无 Json/Text/Yaml；无 protobuf。
- 不把 Pickle 并入 `base`。
- 构建产物只在仓库根 `out/`。

## File map

| Path | Responsibility |
| --- | --- |
| `src/base/archive/archive.h` | `base::` BinarySink / Serializer / archiver |
| `src/base/archive/BUILD.gn` | GN `source_set("archive")`；无 → net |
| `src/net/pack/pickle.h` | `net::Pickle`；deps → `base/archive` |
| `src/net/pack/archive.h` | 兼容 shim（`using` 别名；勿经此特化 traits） |
| `src/base/ipc/codec.h` | encode/decode → `base::Serializer` |
| `src/net/rpc/wire.h` | `base::binary_format_traits<RpcMessage>` 特化 |

---

### Task 1: 落盘 `base/archive` 与 GN

- [x] **Step 1:** 新增 `src/base/archive/`，符号进 `base::`。
- [x] **Step 2:** GN 目标可被 `net/pack` 与 `base/ipc` 依赖；**不**依赖 `net`。
- [x] **Step 3:** Copyright 2026 Mogu；公共面两层命名空间。

### Task 2: 切断 net archive；Pickle 改依赖

- [x] **Step 1:** `pickle.h` 改为 `#include "base/archive/archive.h"`。
- [x] **Step 2:** `src/net/pack/archive.h` 改为兼容 shim。
- [x] **Step 3:** `wire.h` 在 `namespace base` 特化 `binary_format_traits<RpcMessage>`。

### Task 3: `base/ipc` codec

- [x] **Step 1:** codec 只依赖 `base/archive`，去掉对 `net/pack/pickle` 的依赖。
- [x] **Step 2:** `base/ipc` 不拉进 net DLL。

### Task 4: 调用方与验收

- [x] **Step 1:** `net_test` 使用 `base::k_binary_wire_max_string_bytes`。
- [x] **Step 2:** `ipc_test` / `net_test` roundtrip 通过。
- [x] **Step 3:** 文档索引与 `src-layout` / net / ipc 设计稿已对齐。

## Done when

- [x] `base/archive` 为二进制原语唯一权威位置。
- [x] `net::Pickle` 仍在 `net/pack`。
- [x] `base/ipc` codec → `base/archive` only。
- [x] 无 Json/Text/Yaml / protobuf 扩张。
