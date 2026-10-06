<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/files`

mogu-aligned file helpers (Windows port of WSL `/home/ccl/dev/src/mogu/base/files`).

| 头 | 角色 |
| --- | --- |
| `read_file.h` | `read_file_to_string` / `list_files`（`std::filesystem`） |
| `mapped_file.h` | 只读 `CreateFileMapping` + `PrefetchVirtualMemory`（mogu `MappedFile`） |
| `file_mmap.h` | 顺序块产出 + 后台 warmup（mogu `FileMMap`） |
| `file_loader.h` | `FileMMap` → `execution::Pipeline` 三阶段加载（mogu `FileLoader`） |

## 选型

| 场景 | API |
| --- | --- |
| DEM bake cache 默认读 | `MappedFile` + `copy_bytes_chunked`（`vista/.../io_pipeline.h`） |
| DEM bake cache FileLoader 备选 | `vista/.../io_file_loader.h`（`BinaryCopyHandler`） |
| 大文件分块流水线解析 | `FileLoader<BlockHandler>` |
| 小文本 / 列表目录 | `read_file_to_string` / `list_files` |

对比实测：`out/Debug/dem_io_benchmark.exe`（`//src/vista/terrain:dem_io_benchmark`）。
