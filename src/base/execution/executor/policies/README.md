# `base/execution/executor/policies`

Decorator / policy executors wrap an underlying executor (or queue work
and drain later). They are **not** part of the L1 production surface
(`execution_executor.h`).

| Header | Role |
| --- | --- |
| `batch_executor.h` | `BatchExecutor`: wrap an executor; `post_batch` runs a vector as one work item |
| `lazy_executor.h` | `LazyExecutor`: `emplace` then `run` (two-phase) |

Optional backends live under `executor/device/` (GPU / Lane),
`executor/io/` (io_uring / IoLane), and `executor/pool/` (numa / simd
aliases) — they are not decorator policies.

Include these headers via `executor/policies/{batch,lazy}_executor.h`
(or the convenience umbrella `executor/executor.h`). Root
`executor/{batch,lazy}_executor.h` forwarders are gone.

Do not add `policies/` to `execution_executor.h`.
