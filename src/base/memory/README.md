<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/memory`

mogu-aligned foundation memory (header-mostly). GN: `//src/base/memory:memory`
(public_dep of `//src/base:foundation`).

| Header | Role |
| --- | --- |
| `memory_resource.h` | `MemoryResource` + PMR backends (`kNewDelete` / `kMonotonicBuffer` / `kUnsynchronizedPool` / `kSynchronizedPool` / `kHybridOptimized`). **No `kProtobuf`.** |
| `arena.h` | Process + TLS arenas; `base::allocate` / `tls_allocate` / `realloc` |
| `allocator.h` | `ObjectAllocator`, `base::create`, `STLAllocator` |
| `object_pool.h` | Free-list `ObjectPool<T>` → `shared_ptr` recycle |
| `singleton.h` / `scope_guard.h` / `noncopyable.h` / `allocation_tracker.h` | RAII / diagnostics |

## Deps (thin)

| Path | Role |
| --- | --- |
| `base/concurrency/queue.h` | `PushOnlyQueue` via third_party moodycamel |
| `base/synchronization/align.h` | `hardware_destructive_interference_size` |
| `base/traits/{is_detected,concept,class_traits}.h` | `_DestructorSkippable` |

## Adoption

1. **Present / frame** — per-build monotonic / TLS scratch in layout + frame cache; Pass reuses draw scratch.
2. **GIS model** — tileset / OGR decode hot paths use Arena / ObjectPool where lifetimes are batch-scoped.

Test: `memory_test` (`build.bat memory_test`).
