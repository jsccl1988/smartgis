<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# build �?GN/Ninja toolchain

对齐 mogu `build/`（经 mgis �?Windows 适配）：全局 args、toolchain、默�?`config`。日常入口是 **MSVC v145**�?
## 职责

任何 compiler/linker 选项、平台切换、第三方 **路径约定** 应在此修改，而不是写进各个模�?`BUILD.gn`�?
## 目录

| Path | 说明 |
| --- | --- |
| `BUILDCONFIG.gn` | `default_toolchain`、`is_debug` / `is_build_third_party` / `cc_std`（Windows **`c++23`** �?MSVC `/std:c++23preview` on 14.50/14.51, `/std:c++23` when 14.52 ships；Linux/mac `c++23`）、默�?`configs` |
| `config/BUILD.gn` | `default`、`c_std`、`cc_std`、可�?`warnings` |
| `config/win/` | MSVC 默认 flags、CRT、subsystem |
| `BUILD.gn` | `legacy`（MBCS + 2010 include 树） |
| `smartgis.gni` | `product_shared_library` 模板 |
| `toolchain/` | win/linux/mac toolchains |
| `tools/` | `cmake.gni` / `makefile.gni` �?Python 驱动 |
| `fetch_binaries.py` | 拉取 `gn` / `ninja` �?`build/bin` |

## 常用操作

```bat
build.bat
build.bat debug
build.bat release
build.bat debug te
```

默认 **同时** gen/ninja **`out/Debug`**（`is_debug=true`）与 **`out/Release`**（`is_debug=false`）。`build.bat debug|release …` 只编一套。`te` / `e2e` 跑测只使�?**Debug** 产物�?
```bat
gn gen out/Debug --args="is_debug=true is_build_third_party=false"
gn args out/Debug --list
ninja -C out/Debug all
```

入口脚本：仓库根 `build.bat`（可�?`debug` / `release`、`m` / `te` / `a` / `b` / `app` / `views`）。文档索引：[`docs/README.md`](../docs/README.md)。产物约定：[`.cursor/rules/build/build-output.mdc`](../.cursor/rules/build/build-output.mdc)�?
### 多对话编译锁

`build.bat` �?`build/config/win/with_build_lock.ps1` **只锁编译**（`gn`/`ninja`、以�?`t`）。`te` / `e2e` **跑测在放锁之�?*�?
| �?| 覆盖 |
| --- | --- |
| `out/.build.lock.debug` | `out/Debug` ninja |
| `out/.build.lock.release` | `out/Release` ninja |
| `out/.build.lock.shared` | `build.bat t`（与任一 config 互斥�?|
| `out/.build.lock` | 当前 holder 摘要（非互斥本体�?|

调度：每 scope **公平 ticket 队列**（`out/.build.waiters/`）；多目标自�?*按目标拆持锁**（同 ticket 保优先级）；`debug` �?`release` 可并行。成功编译写 `out/.build.last_ok`�?
**全局开关（默认关锁，单人更快）�?*

| 开�?| 效果 |
| --- | --- |
| *(默认)* | **�?* |
| `out/.build.lock.on` | 全局开锁（删文件恢复默认关�?|
| `SMARTGIS_BUILD_LOCK=1` | 本进程开�?|
| `SMARTGIS_BUILD_LOCK=0` | 强制关锁（覆�?sentinel�?|

关锁后：无排队、无拆目标，一次直�?`build.bat`�?
| 环境变量 | 含义 |
| --- | --- |
| `SMARTGIS_BUILD_LOCK` | `1/on` 开；`0/off` 强制关；未设则看 `out/.build.lock.on`（无则关�?|
| `SMARTGIS_BUILD_LOCK_WAIT_SEC` | 最长等待秒数（默认 1800；`0` = 立即失败�?|
| `SMARTGIS_BUILD_OWNER` | 开锁时写入标签（建议必填） |

忙时退出码 **3**。规范：[`.cursor/rules/build/build-lock.mdc`](../.cursor/rules/build/build-lock.mdc)�?
## �?mogu 的差异（有意保留�?
- **�?*使用 Bazel dual-build。GN �?`third_party/.install`；`build.bat t` �?`third_party/tools/batch.py` �?prefix（对�?mogu `build.sh build t`）。`out/third_party`（及�?config 下的 `third_party`）可 junction �?`.install`（运行时�?DLL�?- **�?*�?sln / `vs2008/` / `branches/` 当工程入口（�?[`docs/README.md`](../docs/README.md)�?- **�?*默认打开 `/W4` �?sanitizers
- `use_fast_debug` 已声明，MSVC 仍用 `/Zi`
- Gen 根是 **`out/Debug` / `out/Release`**（不是裸 `out/`，也不是 `out/Default`�?- Harness 截图 / mark / loop 报告�?**`out/<config>/captures/`**；agent 杂项日志�?**`out/<config>/log/`** �?**`out/scratch/`**（可随时清理�?
新增编译选项时：�?`config/` �?`config()`，再 `configs +=`，不要改单个 target 的裸 flags�?
---

**最后更新：** 2026-09-30
