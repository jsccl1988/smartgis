# SmartGIS

2010–2013 MFC 桌面 GIS。**工程管理已升级为 mogu 式 GN/Ninja**（不是 sln/MSBuild）。

## 快速开始

```bat
build.bat
build.bat te
build.bat e2e
build.bat t
build.bat app
build.bat views
build.bat render
build.bat winui
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
| `//:all` | `m` | `//src:src_all`（DLL；不含 MFC exe / UI） |
| `//:test_all` | `te` | `out/exe_smoke.exe`（`--self-test`；`te` 会设 `smt_build_app` 并重编 leftover `SmartGis.exe`，避免旧 PE 对不上新 GeoCore ABI） |
| `//:e2e` | `e2e` | 五个产品 exe + smoke；编完跑 `exe_smoke --require-all` |
| `//:all_with_tests` | `a` | `all` + `test_all` |
| `//:benchmark_all` | `b` | 预留 |
| （fetch + install） | `t` | `third_party/tools/batch.py` → **`third_party/.install`**（对齐 mogu `build.sh build t`；不是只 fetch） |
| `//:smartgis` | `app` / `smartgis` | `out/SmartGis.exe`（`smt_build_app=true`；MFC Feature Pack `CMFC*`，不在 GN `group("all")`） |
| `//:ui_views` | `views` | Views + Skia chrome → `out/SmartGisViews.exe`（唯一终局入口；工具箱含 `AmboxView` / `ChartView`；leftover `SmartGis.exe` 到 parity 为止；不是 Feature Pack / WinUI / Qt） |
| `//:render` | `render` | `out/SmartGisRender.exe`（`smt_build_render=true`；gpu 进程；不在 `group("all")`） |
| `//:winui` | `winui` | `out/SmartGisWinui.exe`（方案 2 WinUI 3；`smt_build_winui=true`；`src/app/winui`；不在 `group("all")`） |

## 目录（对照 mogu）

| mogu | smartgis |
| --- | --- |
| `base/` `common/` …（模块在根下） | `src/` 五层：`app` / `content` / `sdb` / `render` / `base`（对照 `docs/build/src-layout.md`） |
| `build/` + `build.sh` | `build/` + `build.bat` |
| `third_party/` | `third_party/` |
| `out/` | `out/` |
| `BUILD.gn` `.gn` | 同 |
| Bazel 双轨 | **不搬**；本仓只有 GN |

文档索引：[`docs/README.md`](docs/README.md)。对照表 [`docs/build/mogu-mapping.md`](docs/build/mogu-mapping.md)。桌面 UI 终局：Chromium-style Views + Skia（`src/ui/views`、`src/render/skia`），地图仍挂现有 C++ viewport（[`docs/build/ui-views-skia.md`](docs/build/ui-views-skia.md)）。地图/三维 GPU：`src/render/rhi` Facade，底层 FlyCube（DX12 + Vulkan）；同一 CommandList 录 2D 矢量网格 + 3D 模型，`bind_camera` 正交/透视，栅格纹理在 DX12 上采样。GDI/GL leftover 共用 `SmtRender` 进程级 session。模型/场景在 `sdb`。三层深度（模型 / 渲染 / 计算）：[`docs/superpowers/specs/2026-09-13-model-render-compute-design.md`](docs/superpowers/specs/2026-09-13-model-render-compute-design.md)；RHI 实现契约：[`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)。`src/plugin` 是扩展平台（`plugin::Registry` / store / Python），不只是 leftover `*.am`。UI 壳 + 多进程渲染：[`docs/build/ui-shell-multiprocess.md`](docs/build/ui-shell-multiprocess.md)。

文件名跟 mgis/Chromium：`snake_case` 词干；新树用 `.cc` 与 `"content/public/map_view.h"` 式 include；遗留模块保留 `.cpp`。不改 `Smt_*` ABI / `dll_stem`。详表：[`docs/build/src-layout.md`](docs/build/src-layout.md)。GN 布局：[`build/README.md`](build/README.md)。

## 硬依赖

- VS 18 + MSVC **v145**、Windows SDK（GN 用 `cl`，不经过 sln）
- 产品 C++ 标准：**C++23**（`cc_std = "c++23"` → MSVC `/std:c++23`）。不要把新代码写成 C++17。`third_party/` 不跟 `cc_std`。遗留 2010/MFC 按 C++23 编译，但不改写成 concepts。
- **MFC Feature Pack**（VS 18 组件 `Microsoft.VisualStudio.Component.VC.ATLMFC` 或 `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`；MBCS + `afxcontrolbars.h`）：`build.bat app` → `out/SmartGis.exe`。未装时 `//:all` 仍绿。适配头 `src/ui/mfc_ex/bcg_cmfc.h`（`CBCGP*` → `CMFC*` / `CDockablePane`）。**不要**再装 BCGControlBar Pro
- 终局桌面 UI 仍是 Views + Skia，不要改走 WinUI / Qt
- **不要**再装 DirectX June 2010 / D3DX：`src/render/d3d` 已删；GPU 走 FlyCube DX12
- **GDAL/OGR**：数据库数据源走 `datasource/gdal`（`SmtSDEGdalDevice`）；产品只依赖 `//third_party:gdal`（GN 吃 `third_party/.install`，不是 `gdal_sdk`）。ADO 源码已移除。Pin 是 `third_party/manifest.json`（GIS 复用 mgis Gitea；`git_url` 一律 `http://localhost:3000/ccl/<lib>.git`）。源码只落在 **`third_party/.src/`**（fetch 或本地 junction → mgis），顶层 `third_party/<name>/` 只留薄 GN。`build.bat t` → `third_party/tools/batch.py` 拉并装到 **`third_party/.install`**（对齐 mogu `build.sh build t`）。本地 fallback：`.install` 可以是 junction → `c:\Dev\src\gis\mgis\out\third_party`（原先 `gdal_sdk` 那套二进制）。`out/third_party` 可以是指向 `.install` 的 junction（mgis 运行时 `DirectoryAsDLLSearchPath`）；GN 只认 `.install`。不在 ninja 里 cmake sqlite3/PROJ/gdal；`is_build_third_party` 已弃用。运行时 `gdald.dll`（及 `geos` / `proj`）由 GN 拷到 `out/`，否则 `SmartGis.exe` 会因 `STATUS_DLL_NOT_FOUND` 起不来

---

**最后更新：** 2026-09-13
