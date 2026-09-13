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
build.bat web
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
| `//:all` | `m` | `//src:src_all`（27 个 DLL：`core` / `algorithm` / `map`+`feature`+`layer` / `render` / `datasource` 等；不含 MFC exe / UI / `web`） |
| `//:test_all` | `te` | `out/exe_smoke.exe`（已有 exe 带 `--self-test`；缺的跳过） |
| `//:e2e` | `e2e` | 五个产品 exe + smoke；编完跑 `exe_smoke --require-all` |
| `//:all_with_tests` | `a` | `all` + `test_all` |
| `//:benchmark_all` | `b` | 预留 |
| （fetch only） | `t` | `third_party/tools/fetch.py --all`（mogu `build t` 的拉取半边；不写 `.install`） |
| `//:smartgis` | `app` / `smartgis` | `out/SmartGis.exe`（`smt_build_app=true`；MFC Feature Pack `CMFC*`，不在 GN `group("all")`） |
| `//:webview2` | `web` / `webview2` | `out/SmartGisWeb.exe`（方案 1 WebView2 chrome；`smt_build_webview2=true`；不在 `group("all")`） |
| `//:ui_views` | `views` | Views + Skia chrome → `out/SmartGisViews.exe`（`smt_build_views`；工具箱 `src/ui/views`，壳 `src/app/views`；不是 Feature Pack / WinUI / WebView2 / Qt） |
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

文档索引：[`docs/README.md`](docs/README.md)。对照表 [`docs/build/mogu-mapping.md`](docs/build/mogu-mapping.md)。桌面 UI 终局：Chromium-style Views + Skia（`src/ui/views`、`src/render/skia`），地图仍挂现有 C++ viewport（[`docs/build/ui-views-skia.md`](docs/build/ui-views-skia.md)）。地图/三维 GPU：`src/render/rhi` Facade，底层 FlyCube（DX12 + Vulkan）；模型/场景在 `sdb`（[`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)）。UI 壳 + 多进程渲染：[`docs/build/ui-shell-multiprocess.md`](docs/build/ui-shell-multiprocess.md)。

文件名跟 mgis/Chromium：`snake_case` 词干；新树用 `.cc` 与 `"content/public/map_view.h"` 式 include；遗留模块保留 `.cpp`。不改 `Smt_*` ABI / `dll_stem`。详表：[`docs/build/src-layout.md`](docs/build/src-layout.md)。GN 布局：[`build/README.md`](build/README.md)。

## 硬依赖

- VS 18 + MSVC **v145**、Windows SDK（GN 用 `cl`，不经过 sln）
- 产品 C++ 标准：**C++23**（`cc_std = "c++23"` → MSVC `/std:c++23`）。不要把新代码写成 C++17。`third_party/` 不跟 `cc_std`。遗留 2010/MFC 按 C++23 编译，但不改写成 concepts。
- **MFC Feature Pack**（VS 18 组件 `Microsoft.VisualStudio.Component.VC.ATLMFC` 或 `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`；MBCS + `afxcontrolbars.h`）：`build.bat app` → `out/SmartGis.exe`。未装时 `//:all` 仍绿。适配头 `src/ui/mfc_ex/bcg_cmfc.h`（`CBCGP*` → `CMFC*` / `CDockablePane`）。**不要**再装 BCGControlBar Pro
- 终局桌面 UI 仍是 Views + Skia，不要改走 WinUI / WebView2 / Qt
- **DirectX June 2010**（`d3dx9math.h` / `D3DXCreateFont`）：`render/d3d`
- **GDAL/OGR**：数据库数据源走 `datasource/gdal`（`SmtSDEGdalDevice`）；`datasource/smf` 仍用同一 `third_party/gdal_sdk`（junction → mgis `out/third_party`，本地 fallback）。ADO 源码已移除。源码真相是 `third_party/manifest.json` + Gitea（`build.bat t` / `third_party/tools/fetch.py`）；junction `gdal` / `PROJ` / `sqlite3` 仍可用。`is_build_third_party=true` 时 cmake 装到 **`out/third_party`**（不是 `.install`）。运行时 `gdald.dll`（及 `geos` / `proj`）由 GN 拷到 `out/`，否则 `SmartGis.exe` 会因 `STATUS_DLL_NOT_FOUND` 起不来

---

**最后更新：** 2026-09-13
