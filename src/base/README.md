<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base`

mogu-aligned **foundation** + product **platform** DLL in one tree.

| 概念 | GN | 磁盘 | 说明 |
| --- | --- | --- | --- |
| Foundation | **`//src/base:foundation`** | 无独立 DLL | `core`/`threading`/`util`/`files`/`memory`/`time` + `archive`/`ipc`；`#include "base/..."` via `//src` |
| 产品平台 DLL | `//src/base:base`（alias `:platform`） | **`platform.dll` / `platform_d.dll`**（`dll_stem=platform`） | leftovers + carto + xml + `sys`/`net`；deps 链入 foundation |
| 兼容别名 | `//:base` / `//core:core` → `:foundation` | 无物理仓库根 `base/` | 新 deps 请写 `//src/base:foundation` |

Nesting is `src/base/<module>`. Product and foundation includes share the `//src`
root (`#include "base/core/log.h"`, `#include "base/core/api.h"`, …).

GN labels `//src/base:core`, `//src/base:base`, `//src/base:platform`,
`//src/sys:sys`, `//src/net:net` are groups that forward to the platform DLL
(`:base`), except foundation which is `:foundation`.

| Module | Tree | GN | Role |
| --- | --- | --- | --- |
| **foundation core** | `core/`（headers） | `:foundation` | `log` / `macros` / `debug` / `export` / `build_config` |
| **threading / util / files / memory / time** | 同名子树 | `:foundation` | mogu 式薄面；**无** mogu `base::mutex` |
| **archive** | `archive/` | `//src/base/archive:archive` | BinarySink / Serializer（A1；平台 DLL `public_deps`） |
| **ipc** | `ipc/` | `//src/base/ipc:ipc` | Named pipe + pickle（static；非 DLL） |
| **core leftovers** | `core/`（sources） | `core_sources` → `:base` | `listener` / `command` / `msg*` / `api` / structs / `core_assert` — **deferred** |
| **carto** | `../sdb/carto/` | `carto_sources` → `:base` | Cartographic pen / brush / `Envelope` |
| **xml** | `../legacy/xml/` | `xml_sources` → `:base` | TinyXML leftover |
| **sys** | `../sys/` | `sys_sources` → `:base` | `SmtSysManager` + `SmtWinService` + `MemShare` |
| **net** | `../net/` | `net_sources` → `:base` | HTTP / RPC (asio + cpp-httplib) |

Layer group: `//src/base:base_all` → `:foundation` + `:base`.

Export macros: GN defines `BASE_EXPORTS` plus `CORE_EXPORTS` / `STYLE_EXPORTS` /
`SYS_EXPORTS` / `NET_EXPORTS` when building the DLL. `#pragma comment(lib)` points
at `platform` / `platform_d`.

## Deferred core leftovers

Do **not** half-move `listener` / `command` / `msg*` / `api` / structs into
`src/tool` or `src/plugin` until callers stop using `#include "base/core/…"`.
Documented debt; consolidator left them here on purpose.

## Design

- Spec: [`docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`](../../docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md)
- Layout: [`docs/build/src-layout.md`](../../docs/build/src-layout.md)

---

**最后更新：** 2026-09-15
