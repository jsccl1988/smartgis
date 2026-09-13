<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# build — GN/Ninja toolchain

对齐 mogu `build/`（经 mgis 的 Windows 适配）：全局 args、toolchain、默认 `config`。日常入口是 **MSVC v145**。

## 职责

任何 compiler/linker 选项、平台切换、第三方 **路径约定** 应在此修改，而不是写进各个模块 `BUILD.gn`。

## 目录

| Path | 说明 |
| --- | --- |
| `BUILDCONFIG.gn` | `default_toolchain`、`is_debug` / `is_build_third_party` / `cc_std`（Windows **`c++23`** → MSVC `/std:c++23preview` on 14.50/14.51, `/std:c++23` when 14.52 ships；Linux/mac `c++23`）、默认 `configs` |
| `config/BUILD.gn` | `default`、`c_std`、`cc_std`、可选 `warnings` |
| `config/win/` | MSVC 默认 flags、CRT、subsystem |
| `BUILD.gn` | `smt_legacy`（MBCS + 2010 include 树） |
| `smartgis.gni` | `smt_shared_library` 模板 |
| `toolchain/` | win/linux/mac toolchains |
| `tools/` | `cmake.gni` / `makefile.gni` 及 Python 驱动 |
| `fetch_binaries.py` | 拉取 `gn` / `ninja` 到 `build/bin` |

## 常用操作

```bat
build.bat
build.bat te
```

```bat
gn gen out --args="is_debug=true is_build_third_party=false"
gn args out --list
ninja -C out all
```

入口脚本：仓库根 `build.bat`（可带 `m` / `te` / `a` / `b` / `app` / `views`）。文档索引：[`docs/README.md`](../docs/README.md)。

## 与 mogu 的差异（有意保留）

- **不**使用 `third_party/.install` / mogu `build.sh build t` 装 prefix / Bazel dual-build。`build.bat t` 只跑 `third_party/tools/fetch.py`
- **不**把 sln / `vs2008/` / `branches/` 当工程入口（见 [`docs/README.md`](../docs/README.md)）
- **不**默认打开 `/W4` 或 sanitizers
- `use_fast_debug` 已声明，MSVC 仍用 `/Zi`

新增编译选项时：在 `config/` 加 `config()`，再 `configs +=`，不要改单个 target 的裸 flags。

---

**最后更新：** 2026-09-13
