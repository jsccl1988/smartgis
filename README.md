# SmartGIS

2010–2013 MFC 桌面 GIS。**工程管理已升级为 mogu 式 GN/Ninja**（不是 sln/MSBuild）。

## 快速开始

```bat
build.bat
build.bat te
```

工具：`build\bin\gn.exe` + ninja。缺省时尝试 `python build\fetch_binaries.py`。

等价手搓（仅排障；日常走 `build.bat`）：

```bat
gn gen out --root=./ --args="is_debug=true is_build_third_party=false smt_run_vs_env_script=false vs_version=180"
ninja -C out all
```

产物只在仓库根 **`out/`**。日志：**`out/build.log`**。

`vs2008/` 与 `branches/` 已删除。工程入口只有 **GN**（`build.bat`）。`build.bat sln` 会拒绝。

## 根目标（对齐 mogu）

| 目标 | 别名 | 含义 |
| --- | --- | --- |
| `//:all` | `m` | `//src:src_all`（32 个 DLL：原 28 + `SmtGroupToolCore` + `SmtSDESmfDevice` + `SmtSDEDeviceMgr` + `SmtGLRenderDevice`；MFC/BCG/D3DX 库已写 BUILD.gn 但未进 `src_all`） |
| `//:test_all` | `te` | 已注册单测（暂无） |
| `//:all_with_tests` | `a` | `all` + `test_all` |
| `//:benchmark_all` | `b` | 预留 |

## 目录（对照 mogu）

| mogu | smartgis |
| --- | --- |
| `base/` `common/` …（模块在根下） | `src/Smt*`（产品源码在 `src/`） |
| `build/` + `build.sh` | `build/` + `build.bat` |
| `third_party/` | `third_party/` |
| `out/` | `out/` |
| `BUILD.gn` `.gn` | 同 |
| Bazel 双轨 | **不搬**；本仓只有 GN |

对照表：`docs/build/mogu-mapping.md`。

## 硬依赖

- VS 18 + MSVC **v145**、Windows SDK（GN 用 `cl`，不经过 sln）
- **MFC**（VS 组件 `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64` + MBCS）：未装时 `SmtXViewCore` 等 MFC 库不能编入 `src_all`（BUILD.gn 已准备）
- **BCGControlBar Pro**：`SmtMFCExCore` / `SmtGuiCore` / `SmtXAMBoxCore` 硬依赖
- **DirectX June 2010**（`d3dx9math.h` / `D3DXCreateFont`）：`SmtD3DRenderDevice`
- **GDAL/OGR**：`SmtSDESmfDevice` 走 `third_party/gdal_sdk`（junction → mgis `out/third_party`）；源码 junction `gdal` / `PROJ` / `sqlite3`，`is_build_third_party=true` 可 cmake 重编

---

**最后更新：** 2026-09-13
