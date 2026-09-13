<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base`

Foundation layer. Nesting is `src/base/<module>`. Product includes use the `//src` root (`"base/core/log.h"`, etc.). Chrome hosts must not include `ipc/`.

| Module | Tree | GN | DLL stem | Role |
| --- | --- | --- | --- | --- |
| **core** | `core/` | `//src/base:core` | `core` / `coreD` | Threads, log, mem, XML, LoadLibrary plugin |
| **style** | `style/` | `//src/base:base` | `style` / `styleD` | Cartographic pen / brush / annotation / symbol + `Envelope`. Not Views, not CSS, not Skia paint |
| **ipc** | `ipc/` | `//src/base/ipc:ipc` | *(source_set)* | Named pipe + pickle. Not a dep of `core` |

Export macros: `CORE_EXPORT` (`core`), `STYLE_EXPORT` (`style`). GN defines `CORE_EXPORTS=1` / `STYLE_EXPORTS=1` when building each DLL.

`matrix2d.h` is a 2D array template (grid buffer), not a transform matrix. Scene math lives in `src/render/math`.
