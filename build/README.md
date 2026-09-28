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
build.bat debug
build.bat release
build.bat debug te
```

默认 **同时** gen/ninja **`out/Debug`**（`is_debug=true`）与 **`out/Release`**（`is_debug=false`）。`build.bat debug|release …` 只编一套。`te` / `e2e` 跑测只使用 **Debug** 产物。

```bat
gn gen out/Debug --args="is_debug=true is_build_third_party=false"
gn args out/Debug --list
ninja -C out/Debug all
```

入口脚本：仓库根 `build.bat`（可带 `debug` / `release`、`m` / `te` / `a` / `b` / `app` / `views`）。文档索引：[`docs/README.md`](../docs/README.md)。产物约定：[`.cursor/rules/build/build-output.mdc`](../.cursor/rules/build/build-output.mdc)。

## 与 mogu 的差异（有意保留）

- **不**使用 Bazel dual-build。GN 吃 `third_party/.install`；`build.bat t` → `third_party/tools/batch.py` 装 prefix（对齐 mogu `build.sh build t`）。`out/third_party`（及各 config 下的 `third_party`）可 junction 到 `.install`（运行时搜 DLL）
- **不**把 sln / `vs2008/` / `branches/` 当工程入口（见 [`docs/README.md`](../docs/README.md)）
- **不**默认打开 `/W4` 或 sanitizers
- `use_fast_debug` 已声明，MSVC 仍用 `/Zi`
- Gen 根是 **`out/Debug` / `out/Release`**（不是裸 `out/`，也不是 `out/Default`）

新增编译选项时：在 `config/` 加 `config()`，再 `configs +=`，不要改单个 target 的裸 flags。

---

**最后更新：** 2026-09-28
