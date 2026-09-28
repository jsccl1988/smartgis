<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/execution`

mogu-aligned task execution (Hybrid). Spec:
[`docs/superpowers/specs/2026-09-28-base-execution-design.md`](../../../docs/superpowers/specs/2026-09-28-base-execution-design.md).

| Need | Include |
| --- | --- |
| Thread pool / inline / IO / global | `base/execution/execution_executor.h` |
| Futures `then` / `when_all` / `async` | `base/execution/futures/combinators/continuation.h` |
| `parallel_for` | `base/execution/parallel/for.h` |
| Pipeline / MapReduce | `base/execution/pipeline/pipeline.h`, `map_reduce/map_reduce.h` |

Windows: portable `IOExecutor` (1-thread pool). `io_uring` / IoLane stubs unless `OS_LINUX` + `HAVE_LIBURING`. `BThreadPool` aliases `NThreadPool` (no brpc). GPU executor falls back to CPU pool.

Test: `ninja -C out execution_test.exe`

---

**最后更新：** 2026-09-28
