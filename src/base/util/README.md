<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/util`

| 头 | 角色 |
| --- | --- |
| `path.h` | `base::path` + `self_path()`（Win：`GetModuleFileNameA`） |
| `library.h` | `base::library`（`LoadLibraryW` / `GetProcAddress`） |
| `plugin.h` | 通用 dynlib plugin + `plugin_manager`；产品宿主见 `src/plugin` |
