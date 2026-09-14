<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/core`

mogu 对齐的编译期 / 日志面（公开符号在 `namespace base`，无 `base::core`；内部放 `base::detail`）。

| 头 | 角色 |
| --- | --- |
| `build_config.h` | OS / 编译器 / CPU 宏 |
| `export.h` | `BASE_EXPORT` |
| `macros.h` | `DISALLOW_COPY_AND_ASSIGN` / `LIKELY` / `ANONYMOUS_VAR` 等 |
| `log.h` | `LOGGING(level, …)`（Windows：`localtime_s` / `GetCurrentThreadId`，无裸 POSIX） |
| `debug.h` | `ScopedTimer` / `DEBUG_ASSERT` / `DEBUG_CHECK`（子集；完整 mogu debug 依赖后续 util） |

Include：`#include "base/core/log.h"`。

遗留 `SmtLog` / `SmtLogManager` 已删除。
