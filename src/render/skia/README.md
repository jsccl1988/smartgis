<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/render/skia` — 壳画布（GDI stub → 可选真 Skia）

Skia 是 Views 壳的 **canvas / paint** 后端，不是 GIS GPU，也不是 widget kit。公开命名空间：`render::skia`。Include：`"render/skia/..."`。

**默认实现是 GDI32 stub**（`canvas.cc`，`libs = gdi32.lib`）。不 vendor 整树 Chromium / Skia；不要 drive-by `git clone` 进 `third_party/`。

最小公开 API 与阶段边界：[`docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md`](../../../docs/superpowers/specs/2026-09-14-render-skia-canvas-design.md)。实现计划：[`docs/superpowers/plans/2026-09-14-render-skia-canvas.md`](../../../docs/superpowers/plans/2026-09-14-render-skia-canvas.md)。架构 as-built：[`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md)。

## GN

| 项 | 约定 |
| --- | --- |
| 目标 | `//src/render/skia:skia` |
| 入口 | 仅经 `//:ui_views` / `build.bat views` |
| **禁止** | 进 `//src:src_all`、`//src/render:render_all`、根 `group("all")` 默认图 |
| 开关 | `smt_has_skia`（`skia.gni`），**默认 `false`** |
| 默认路径 | `canvas.cc` + `gdi32.lib`；无本机 pin 时行为不变 |
| 真后端 | `smt_has_skia=true` → `canvas_skia.cc` + `SMT_HAS_SKIA`；Views **无** `#ifdef` |

```bat
REM 日常 / CI：不要开真 Skia
build.bat views

REM 本机已建 pin + 匹配 Windows skia.lib 时试真后端
REM 在 out\args.gn 增加 smt_has_skia = true 后：
gn gen out --root=./
ninja -C out views_unittests
out\views_unittests.exe --self-test
```

缺 pin 时 GN `assert` 明确失败；缺匹配 `skia.lib` 时 gen 会 WARNING，链接阶段失败。**默认 `false` 不受影响。**

## 本机 pin（与 FlyCube 同纪律）

### 权威源（本机）

| 项 | 路径 |
| --- | --- |
| WSL | `/home/ccl/dev/src/open/topic/graphic-engine/skia` |
| Windows UNC | `\\wsl$\Ubuntu-24.04\home\ccl\dev\src\open\topic\graphic-engine\skia` |
| 亦可 | `\\wsl.localhost\Ubuntu-24.04\home\ccl\dev\src\open\topic\graphic-engine\skia` |

### Junction / 符号链接

Windows **`mklink /J` 不能指向 UNC**（报「本地路径」）。对 WSL 路径请用**提升权限的目录符号链接**：

```bat
REM 管理员 CMD（推荐）
mklink /D third_party\.src\skia \\wsl$\Ubuntu-24.04\home\ccl\dev\src\open\topic\graphic-engine\skia
```

或管理员 PowerShell：

```powershell
New-Item -ItemType SymbolicLink -Path third_party\.src\skia `
  -Target '\\wsl$\Ubuntu-24.04\home\ccl\dev\src\open\topic\graphic-engine\skia'
```

### 预编译库（Windows，本机 MSVC）

真后端链接需要 **与上述 pin 同版本的 Windows `skia.lib`**，约定路径：

```
third_party\.src\skia_out\skia\skia.lib   # GN 探测路径
third_party\.src\skia_out\skia.lib        # ninja 产物（可硬链到上者）
third_party\.src\skia_out\skcms.lib       # 一并链接
```

#### 如何编出（官方 GN/Ninja，本机 MSVC）

1. **不要**在 WSL 用 GCC/Clang 编再链 MSVC——ABI 不兼容。必须在 **Windows + MSVC** 编。
2. 本机 googlesource / CIPD 常不可达；用 **最小 CPU 配置**（关 GPU/codec/PDF），**不需要** `tools/git-sync-deps`。
3. 参数文件：`out/skia_win_args.gn`（仓库内可再生成）。要点：
   - `is_official_build=true`（避免 spirv 等 DEPS）
   - `extra_cflags = ["/MDd", "/EHsc", "/D_HAS_EXCEPTIONS=1"]` —— 对齐 smartgis debug CRT（默认 `/MDd`）；Skia 默认 `/MT` + `_HAS_EXCEPTIONS=0` 会 LNK2001
   - 关 `skia_enable_ganesh` / codecs / zlib 等
   - `win_vc` 显式指向 VS 18（Skia `find_msvc.py` 只认 2017/2019/2022）
4. 生成与编译（需 `python3` 在 PATH、repo `build/bin/gn.exe` + `ninja`）：

```bat
REM 也可：out\rebuild_skia_win.bat
copy /Y out\skia_win_args.gn third_party\.src\skia_out\args.gn
build\bin\gn.exe gen third_party\.src\skia_out --root=third_party\.src\skia
ninja -C third_party\.src\skia_out skia -j 8
mkdir third_party\.src\skia_out\skia 2>nul
mklink /H third_party\.src\skia_out\skia\skia.lib third_party\.src\skia_out\skia.lib
```

日志：`out/skia-gn-gen.log`、`out/skia-ninja-build.log`。

**不要**把其它树（例如旧 skui CMake）的 `skia.lib` 链到本 pin——ABI 不匹配。无匹配 lib 时保持 `smt_has_skia=false`。

3. **禁止**把整树 Skia 提交进 `third_party/skia` 当 vendor。`.src/` 已在 `third_party/.gitignore`。
4. 实现 / CI **不得**靠 GitHub clone 救编译。
5. 开启失败必须 fallback GDI（保持默认 `smt_has_skia=false`）；不得让无 pin 机器上的 `build.bat views` 硬挂。

### 本机现状（2026-09-14）

| 项 | 状态 |
| --- | --- |
| 发行版 | `Ubuntu-24.04`（`\\wsl$` / `\\wsl.localhost` 均可） |
| `third_party\.src\skia` | **目录符号链接** → WSL graphic-engine/skia |
| Windows `skia.lib` | **有**：`third_party\.src\skia_out\skia\skia.lib`（本机 MSVC `/MDd` 最小 CPU 构建） |
| 真后端 TU | `canvas_skia.cc`（`SkSurfaces::WrapPixels` + DirectWrite FontMgr + `BitBlt`） |
| `smt_has_skia=true` 编译 | **通过** |
| `smt_has_skia=true` 链接 | **通过**；`views_unittests --self-test` **ok** |
| 默认 `smt_has_skia` | **false**（GDI stub，`views_unittests` 绿） |

## 真 Skia 准入条件（全部满足才允许默认切）

与 design 一致；任一条不满足 → **保持 GDI stub 为默认实现**。

1. **本机 pin**：仓库内仅 junction / symlink / `args` 路径指向本机 Skia 检出；禁止整树提交进 `third_party/`。
2. **GN 显式开启**：`smt_has_skia=true`；默认 `false`；CI / 日常 `build.bat` 不依赖 Skia 源树。
3. **不进 `src_all`**：`//src/render/skia:skia` 仍只经 `//:ui_views`；真 Skia 目标不得被 `group("all")` 默认拉起。
4. **公开 API 不变**：Views 只 `#include "render/skia/canvas.h"`；无 `#ifdef` 泄漏到 `paint_self`。
5. **测试**：GDI stub 与真 Skia（若本机开启且能链接）均能跑通同一套 canvas 行为测试（像素容差可放宽到非空 / 尺寸正确；不做位图黄金图）。
6. **许可证 / 构建**：Skia 构建脚本与依赖清单写在本 README；失败时 fallback GDI，不硬 fail 整仓。

### 链接依赖（真后端，有匹配 lib 时）

- `skia.lib` + `skcms.lib`（与 pin 同源的 **Windows MSVC** 构建，CRT 须与产品一致，当前 debug 为 `/MDd`）
- 系统：`usp10` / `fontsub` / `gdi32` / `user32` / `ole32` / `oleaut32` / `dwrite` / `windowscodecs`

阶段 D 已提供 `canvas_skia.cc`、GN 切换与本机 `skia.lib`；**默认实现仍是 GDI stub**。

---

**最后更新：** 2026-09-14
