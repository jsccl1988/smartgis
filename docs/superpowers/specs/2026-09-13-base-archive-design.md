<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Base archive：二进制 BinarySink / Serializer 上提（A1）

**Date:** 2026-09-13  
**Status:** accepted  
**Scope:** A1 **仅**上提二进制 archive（`BinarySink` / `Serializer` / `InArchiver` / `OutArchiver` 及同类 helpers）。**不含** Json / Text / Yaml sink；**不含** protobuf / 第二套 IDL。  
**Related:** [`2026-09-13-net-asio-httplib-design.md`](2026-09-13-net-asio-httplib-design.md)、[`2026-09-13-base-ipc-mojom-design.md`](2026-09-13-base-ipc-mojom-design.md)。实现计划见 [`../plans/2026-09-13-base-archive.md`](../plans/2026-09-13-base-archive.md)。

## Goal

把可复用的二进制序列化原语从 `src/net/pack/archive.h`（命名空间 `net`）上提到 `src/base/archive/archive.h`（命名空间 `base`），对齐 mogu 的 **`base/archive` + `net/pack`** 切分：

| 能力 | 路径 | 命名空间 |
| --- | --- | --- |
| BinarySink / Serializer / archive helpers | `src/base/archive/` | `base` |
| Pickle（包络 / RPC 侧） | `src/net/pack/pickle.h` | `net` |

`base/ipc` 的 codec 依赖 **`base::archive` helpers**，不再依赖 `net` 的 archive 头。线上字节布局（wire）**不变**。

## Non-goals

- 不上提、不新增 Json / Text / Yaml archive 变体。
- 不引入 protobuf、mojom 生成器，或第二套 IPC IDL。
- 不把 `net::Pickle` 并入 `base`；Pickle 留在 `src/net/pack/pickle.h`。
- 不借此机会改 FnRPC / named-pipe 帧格式或 `archive()` 字段顺序。
- 不改 `dll_stem`、不破现有 `Smt_*` ABI（除非同树并行的 ABI cutover 另有规定）。
- Qt 禁止。

## Architecture（对齐 mogu）

```
callers (base/ipc codec, net/pack/pickle, net/rpc, …)
        |
        |  #include "base/archive/archive.h"
        v
src/base/archive/          namespace base
        BinarySink / Serializer / InArchiver / OutArchiver
        （二进制原子读写；无 Json/Text/Yaml）

src/net/pack/pickle.h      namespace net
        Pickle — 依赖 base/archive，不反向依赖 ipc

src/base/ipc/              namespace base::ipc（两层公共面；内部 detail）
        named pipe + frame envelope；codec 用 base::archive
```

与 mogu 一致：**序列化原语在 `base/archive`，网络 Pickle 在 `net/pack`**。本仓产品树在 `src/` 下，故路径为 `src/base/archive/`、`src/net/pack/`（不是仓库根 `base/`）。

## Namespace 与 include

- 公共符号在 **`base`**（至多两层；更深进 `base::detail`）。
- Include：`#include "base/archive/archive.h"`（或同目录公开头）。
- `net::Pickle` 继续 `#include "net/pack/pickle.h"`，其实现/头依赖 `base/archive`。

## 依赖图

```
base/archive          （无 → net / ipc）
      ^
      |  deps
      +-- net/pack/pickle
      +-- base/ipc（codec）
      +-- （可选）net/rpc 等已用 BinarySink 的调用方，经 include 改路径
```

- **`//src/base/archive`**（或等价 GN 目标）不依赖 `net`、不依赖 `ipc`。
- **`net/pack/pickle`** → `base/archive`。
- **`base/ipc`** → `base/archive`（**不是** `net/pack/archive`）。

## Wire

- BinarySink 原子、字符串 `size_t` 长度前缀、消息 `archive()` 顺序与 endian 约定 **保持现状**。
- named-pipe 帧信封、FnRPC CRLF + pickle 载荷语义 **不变**；只换头文件与命名空间归属。

## Migration notes

1. 新建 `src/base/archive/`（`archive.h` 等）与 GN 目标；符号迁入 `base::`。
2. 删除或瘦身 `src/net/pack/archive.h`：不再放 BinarySink/Serializer；`pickle.h` 改依赖 `base/archive`。
3. `base/ipc` codec：include / deps 从 `net` archive 改为 `base/archive`。
4. 调用方全局替换 `#include "net/pack/archive.h"` → `"base/archive/archive.h"`，`net::BinarySink` 等 → `base::…`（以实际符号表为准）。
5. 单测：既有 pickle / RPC / IPC roundtrip 仍过；不新增 wire 兼容矩阵。
6. 文档：`src-layout`、`net` / `ipc` 设计稿与本 spec 对齐（本变更集）。

## Self-review

1. A1 边界清晰：只有二进制 archive；Json/Text/Yaml / protobuf 显式排除。
2. 切分与 mogu `base/archive` + `net/pack` 一致；Pickle 留在 net。
3. Wire 声明为不变；回归靠现有 loopback / IPC 测试。
