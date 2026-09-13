<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base`

Foundation layer. Nesting is `src/base/<module>`. Product includes use the `//src` root (`"base/core/log.h"`, etc.). Chrome hosts must not include `ipc/`.

One shared library: **`dll_stem = base`** (`base.dll` / `base_d.dll`). GN labels `//src/base:core`, `//src/base:base`, `//src/sys:sys`, `//src/net:net`, and `//src/base/ipc:ipc` are groups that forward to `//src/base:base`.

| Module | Tree | GN | Role |
| --- | --- | --- | --- |
| **core** | `core/` | `core_sources` → `//src/base:base` (alias `//src/base:core`, `//core:core`) | Threads, log, mem, XML, LoadLibrary plugin |
| **style** | `style/` | `style_sources` → `//src/base:base` | Cartographic pen / brush / annotation / symbol + `Envelope`. Not Views, not CSS, not Skia paint |
| **sys** | `../sys/` | `sys_sources` → `//src/base:base` | `SmtSysManager` |
| **net** | `../net/` | `net_sources` → `//src/base:base` | HTTP / RPC (asio + cpp-httplib) |
| **archive** | `archive/` | `//src/base/archive:archive` | BinarySink / Serializer (header-only; public_deps of base) |
| **ipc** | `ipc/` | `ipc_sources` → `//src/base:base` | Named pipe + pickle |

Layer group: `//src/base:base_all` → `:base`.

Export macros (migration): GN defines `BASE_EXPORTS` plus `CORE_EXPORTS` / `STYLE_EXPORTS` / `SYS_EXPORTS` / `NET_EXPORTS` when building the DLL. Headers keep `CORE_EXPORT` / `STYLE_EXPORT` / … ; `#pragma comment(lib)` points at `base` / `base_d`.

`matrix2d.h` is a 2D array template (grid buffer), not a transform matrix. Scene math lives in `src/render/math`.
