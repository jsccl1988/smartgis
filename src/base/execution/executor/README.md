# `base/execution/executor`

Command-style `execute()` track. L1 is **not** every header here — see
`execution_executor.h`. Convenience dump: `executor.h` (L1 + optional CPU
aliases + GPU; **not** io_uring / IoLane).

```text
executor/
  contexts/     ThreadContext (nthread / bthread / gthread)
  policies/     BatchExecutor, LazyExecutor (decorators; not L1)
  pool/         CPU pools (L1) + numa / simd aliases
  io/           IOExecutor (L1), IOUringExecutor, IoLane
  device/       GPUExecutor, DeviceLane, DeviceColumn, LaneSet, …
  executor.h    umbrella
```

| Need | Include |
| --- | --- |
| L1 production | `base/execution/execution_executor.h` |
| Optional CPU | `executor/pool/{numa,simd}_executor.h` |
| Storage SQE | `executor/io/io_lane.h` (not `IOUringExecutor`) |
| GPU / Lane | `executor/device/…` |
| Decorators | `executor/policies/{batch,lazy}_executor.h` |

Namespace stays `base::execution` (no third public layer).
