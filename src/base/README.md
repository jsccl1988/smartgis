<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base`

mogu-aligned **foundation** + product **base** DLL in one tree.

| 概念 | GN | 磁盘 | 说明 |
| --- | --- | --- | --- |
| Foundation | **`//src/base:foundation`** | 无独立 DLL | `core`/`threading`/`util`/`files`/`memory`/`time` + `archive`/`ipc`；`#include "base/..."` via `//src` |
| 产品平台 DLL | `//src/base:base`（legacy alias `:platform`） | **`base.dll` / `base_d.dll`**（`dll_stem=base`） | leftovers + carto + xml + `sys`；deps 链入 foundation；HTTP/RPC 在 `//src/net:net` |
| 兼容别名 | `//:base` / `//:core` → `:foundation` | 无物理仓库根 `base/`、`core/` | 新 deps 请写 `//src/base:foundation` |

Nesting is `src/base/<module>`. Foundation includes stay under `base/…`
(`#include "base/core/log.h"`). Leftover Smt core headers live under
`legacy/core/…` (`#include "legacy/core/api.h"`).

GN labels `//src/base:core`, `//src/base:base`, `//src/base:platform`,
`//src/legacy/core:core`, `//src/legacy/sys:sys` are groups that forward to the
base DLL (`:base`), except foundation which is `:foundation`.
`//src/net:net` is a separate product DLL (`net.dll` / `net_d.dll`).

| Module | Tree | GN | Role |
| --- | --- | --- | --- |
| **foundation core** | `core/`（headers） | `:foundation` | `log` / `macros` / `debug` / `export` / `build_config` |
| **threading / util / files / memory / time** | 同名子树 | `:foundation` | mogu 式薄面；**无** mogu `base::mutex` |
| **synchronization / concurrency** | `synchronization/`、`concurrency/` | `:foundation` | latch/future/spin + moodycamel queues |
| **execution** | `execution/` | `//src/base/execution:execution` → foundation | L1–L5 + `parallel_for` / Pipeline / MapReduce；Windows 可移植 IO；io_uring 门控 |
| **trace** | `trace/` | `//src/base/trace:trace` → foundation | Chrome Trace `Trace` / `ScopedTracer` / `SpanRecorder` / `export_chrome_trace`；`SMT_TRACE=1` |
| **archive** | `archive/` | `//src/base/archive:archive` | BinarySink / Serializer（A1；平台 DLL `public_deps`） |
| **ipc** | `ipc/{codec,handle,channel,endpoint,data_pipe,invitation,portal,receiver}` | `//src/base/ipc:ipc` | Named pipe + pickle + invitation / DataPipe / Node+Portal / PendingRemote（mojom 形状，无 IDL；static；非 DLL）。头与实现同目录，例如 `#include "base/ipc/channel/channel.h"` |
| **math** | `math/` | `//src/base/math:math`, `:bounds` | Scene Vector/Matrix/Aabb (namespace `render`). Source sets only; **not** in `base.dll` |
| **carto** | `../legacy/carto/` | `carto_sources` → `:base` | Leftover pen / brush / `SmtStyle` / `StyleManager`（**not** `Envelope` — see `gis/model/envelope.h`） |
| **legacy core** | `../legacy/core/` | `core_sources` → `:base` | `listener` / `command` / `msg*` / `api` / structs / `core_assert` / `core.h` |
| **xml** | `../legacy/xml/` | `xml_sources` → `:base` | TinyXML leftover |
| **sys** | `../legacy/sys/` | `sys_sources` → `:base` | legacy `SmtSysManager` only（`MemShare` / `SmtWinService` removed — unused） |
| **net** | `../net/` | `//src/net:net` | HTTP / RPC DLL (asio + cpp-httplib); not in `base.dll` |

Layer group: `//src/base:base_all` → `:foundation` + `:base`.

Export macros: GN defines `BASE_EXPORTS` when building the base DLL.
`#pragma comment(lib)` points at `base` / `base_d`. Net uses `NET_EXPORTS` /
`net` / `net_d` via `net/net_export.h`.

## Design

- Spec: [`docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`](../../docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md)
- Layout: [`docs/build/src-layout.md`](../../docs/build/src-layout.md)

---

**最后更新：** 2026-09-28
