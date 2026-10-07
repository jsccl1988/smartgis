<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` — Scheme 3 shell (SmartGIS Horizon)

**Diagram:** [`docs/superpowers/diagrams/ui-views-shell-architecture.html`](../../../docs/superpowers/diagrams/ui-views-shell-architecture.html)（shell / compositor 泳道 + 流水线）

**Brand:** **SmartGIS Horizon**（次世代桌面 GIS）。工程目录是 `src/app/views/`（`app/` · `browser/` · `ui/` · `il.runtime/` · `util/`），不是 Chromium 的 `chrome/`。Living lock: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md) §Horizon product brand.

Product shell for **Views + Skia**。`SmartGIS.exe` 是宿主：`Widget` +
layout + 公开 `ui::views` 控件 + 命令接线。不手绘 catalog / feature / status。

单窗口 IDE 布局（非 MDI）：

```
ui::views::Widget
  RootView  BoxLayout vertical
    MenuBar          File / Edit / View / Layer（下拉，无平铺按钮）
                     View 与地图右键共用导航表；其后分隔线 + Refresh /
                     Toggle Diagnostic Tools / RHI / MapLibre
                     Layer：创建、底图、移除、缩放到图层
    Splitter vertical (flex)
      Splitter horizontal
        BoxLayout vertical
          AmboxView (horizontal tool bar: Select | Edit | Tools)
          Splitter horizontal
            CatalogView (~288; Layers|Sources|Maps tabs at bottom)
            TabStrip (flex; tabs at bottom): Map | Data | 3D
              各页 DrawHost（非活动 HWND 隐藏）
        TabStrip right dock (~280): AMBox | FeatureInfo | AttributeTable | …
      DiagnosticToolsPanel（底栏，默认开启；active=Console）
        tabs: Output | Console | Trace | Memory
    StatusBar
```

Debug Console / LogSink / Agent / Python worker：见 living shell
**§Diagnostic Tools** / archived
[`docs/superpowers/archive/specs/2026-09-28-debug-console-design.md`](../../docs/superpowers/archive/specs/2026-09-28-debug-console-design.md)。
产品壳默认打开底栏 Console；菜单 View → Toggle Diagnostic Tools 可折叠。
三个地图页各自一个 `DrawHost` + `content::ViewHost`（2D 编辑 / 2D 浏览 /
3D）。`MapContents` 会话共享；`OpenView` 分别为 `kMapEdit` / `kMapData` /
`kScene3d`。3D 若无法挂接则保持 native 占位，鼠标不崩。

源码按职责分目录（无根目录转发头）。Chromium 分层见 living shell spec
**§Content sink**：`app/views` 根下直接是壳子树（≈ horizon）。`Browser` 持有
`content::MapSession`（≈ WebContents：拥有 `MapScene` / camera / present /
gestures / ViewHosts / `MapContents*`）；能力实现在
`src/content/browser/{document,camera,present,input}`；GDI paint 在
`content/browser/present/*/software/`。`ui/` → `browser/` →
`//src/content:map_session`；**禁止** `present` → `app/views`。对等目录：`app/`、
`browser/`、`ui/`、`il.runtime/`、`util/`。
`il.runtime/bind/` 是机制（`host_member` traits + `bind_tagged_slots` +
`reflect_fields` + `named_find` / `bind_into`）。
`il.runtime/frontend/` 是语法和源文件查找：`Interact.g4` + AST + parse + `load`（suite id → `.il`）。`load` 不 include `backend/`。
`il.runtime/ir/` 是语言无关指令（`app::ir`）。
`il.runtime/backend/` 打平。GN `:compile` 是降级和解释（`driver`、`apply`、`lower_*`、`exec`、`eval_host`）。GN `:capability` 是 Host 原语（`bind_host`、`bind_horizon`、`bind_plugin`、`bind_export`、`capture_host`、`shell_expect`）。`:il.runtime` 是 session（`run_script` / `interact_script`）：链接一次再 apply。`:compile` 不依赖 `:capability`。
`browser/plugin/` 是 present / playback / preview / `report_suite` 缝。
Harness 收口（living shell **§Harness IL capability cut**）：HWND / pump / capture 原子在 `il.runtime/backend/`；浏览器集成测试在 `testing/tools/harness/browser/browser.{harness,console,map2d.*,world3d.*,input}/*.il`；`app/startup/` 是 LaunchPolicy 登记表（`scenario.*`）；`src/app/views/harness/` 已删。
图：[`docs/superpowers/diagrams/views-runtime-layers.html`](../../../docs/superpowers/diagrams/views-runtime-layers.html) · [`docs/superpowers/diagrams/harness-il-capability.html`](../../../docs/superpowers/diagrams/harness-il-capability.html)。`ui/`：`BrowserView`
持有 Widget 树字段 + `BrowserUiDelegate` 薄转发；独立子目录
`shell/`（`ShellLayoutComposer` / `ShellLifecycleComposer`）、
`horizon/`（`MenuComposer` / `CatalogComposer` / `AmboxComposer` /
`InspectorHostComposer`）、`pages/`（`MapPagesComposer` 多 TU）、
`panels/`（`ProcessingComposer` / `InspectComposer` /
`InspectorSyncComposer` / `DebugConsoleComposer`）。见 living shell
**§shell/ui composers**。`app/main.cc` 仅 `wWinMain` 胶水。
Present README：
[`../../content/browser/present/README.md`](../../content/browser/present/README.md)。

`wWinMain`（`app/main.cc`）→ CLI11 解析 → `content::content_main`（`process_type_set`），
再进 `browser_main` / `gpu_main` / `renderer_main`。同一 PE 以 `--type=gpu`
/ `--type=renderer` 再拉起。地图挂接仍走 `DrawHost::attach()`；原生 HWND
把鼠标 / 键 / 滚轮转给 `ViewHost::dispatch_input`。

```bat
build.bat views
```

产出 `out/SmartGIS.exe`（`build_views=true`）。不在
`group("all")` 里。`--self-test` 泵消息、检查 widget HWND，切换 Map/Data/3D
页，在 `kContentMapView` 时 `wait_ready`，并对 3D 页跑 `view3d.trackball`
输入（无 GPU 时占位 HWND 亦可）。分层与退出码：
[`docs/superpowers/ui-testing.md`](../../../docs/superpowers/ui-testing.md)。

Open：`MapScene::open_path` 走 **OGR**（GPKG / Shapefile / GeoJSON 等）把真实
图层名与几何灌进 Catalog / 2D overlay；打不开时才回退样例要素。启动时
`seed_default` / 自测优先加载 **`china_city.gpkg`**（NE 10m 四层：`area` /
`line` / `point` / `text`，EPSG:4326）；3D 用同 CRS 的 **`china_dem.tif`**。
缺失时回退 `china_plp.geojson`。样例数据：

- 仓库：`testing/data/china_city.gpkg`（构建复制到共享 `out/data/`；同目录有匹配的 `china_city.geojson`）
- 高程：`testing/data/china_dem.tif`（AWS terrain tiles → EPSG:4326，NE 陆地裁切）
- 生成：`py -3 testing/data/build_china_city.py --with-dem`（Natural Earth 矢量 + DEM）
- 许可 / PIN：`testing/data/china_city.LICENSE.txt`、`china_city.PIN.txt`
- 兜底：`testing/data/china_plp.geojson`
- 自测：优先 `out/data/china_city.gpkg` / `.geojson`（≥4 层或 kind 四分、要素量级远高于示意 PLP）

菜单 **Open** 或 Catalog「加载 shp」选上述文件即可；状态栏显示 `Opened (OGR): …`。
图层右键 **View** 缩放到全图。菜单 **DrawLine** = `edit.append.linestring`；
**Save** = 将当前 active 可见层写出为 GeoJSON（`MapScene::write_path`）。

地图页是 **共享场景宿主**：`MapContents::OpenView`（`kMapEdit` / `kMapData` /
`kScene3d`）+ `SetExtent`（有中国范围则全幅中国）。2D 为正交，3D 为透视。
手势：滚轮对光标缩放、平移；HWND 允许时双指捏合（`WM_GESTURE` / 指针）。

**启动默认与 showcase 对齐（观感，非 GDI 强制）**：`china_product_defaults`
（`browser/`）供交互 shell 与 `--map2d-showcase=china` /
`--atmosphere-showcase=full` 共用——中国样例清掉 `china_city.style.json`（默认
carto）、mainland 取景、`kChinaLonLatExtent` + orbit `distance=2.55`、3D 大气
ocean/cloud/sky/**fog**（`SCENE3D_ATMO=0` / `SCENE3D_LAND_ONLY=1` 可关）。
产品默认 2D = Vista/FlyCube GPU SoT（`product_startup_policy`）；harness /
ContentGdi / `FORCE_*` 才钉 ContentMapView/GDI。

**2D 主路径 = RHI**：Map/Data 页默认 `kGpuPresent`；`Map2dPresenter::present_gpu`
→ Vista `Layout`/`MapPass`。成功且 present HWND 可见时，shell overlay 只画
`paint_annotation_overlay`；失败或 `FORCE_GDI_MAP_OVERLAY=1` 时回退全量 GDI
`Map2dPresenter::paint`。

```bat
rem 强制 2D 走 ContentMapView / 跳过 Vista：
set FORCE_CONTENT_MAPVIEW_2D=1
rem 或: set PREFER_FLYCUBE_2D=0
rem 强制 GDI 全量 overlay（仍可挂 Vista HWND，但不以 present_gpu 为 SoT）：
set FORCE_GDI_MAP_OVERLAY=1
out\Debug\SmartGisViews.exe
```

3D 页：`view3d.trackball` 更新 `OrbitFrame` / `Scene3dPresenter`。默认
**Vista RHI**（`present_gpu`；成功时 shell 只叠 `paint_hud`）。挂接失败时
回退 ContentMapView / GDI `Scene3dPresenter::paint()`。**不会**在 Vista
SoT 下再挂 leftover OpenGL（同 HWND 抢 swapchain 会把徽章永久钉成
`Stereo/GL`）。

**手动切换 3D 引擎**（不经环境变量）：菜单 **View → Engine: Vista/DX12 /
Stereo/GL / GDI**，或 `content::set_scene3d_engine(...)`。命令 id：
`view.engine.flycube` / `view.engine.stereo_gl` / `view.engine.gdi`。切换时
会 detach/reattach Scene3d `DrawHost`，并按选择挂放 stereo。

3D HUD 显示引擎名；画面**右下角**有引擎 Logo 徽章（与真实后端一致：
`Vista/DX12` / `Stereo/GL` / `GDI` / `ContentMapView` / `Null`）。DEM 默认
叠 hypsometric 着色；若存在 `china_rs.tif` / `china_imagery.tif`（exe 旁或
`testing/data/`）则 draping 遥感影像。TIN 线框：

```bat
set SCENE3D_WIREFRAME=1
out\SmartGIS.exe
```

`--self-test` 会 `set_scene3d_engine(kGdi)`（挂起规避），并断言 OGR 进层与相机矩阵；若挂上
Vista 会写 `flycube-camera-ok`，并在 present 前开 `enable_atmosphere_demo()`。

大气 3D 端到端 showcase。默认 **Null RHI**（可重复退出 0）；
真 GPU：`set ATMOSPHERE_SHOWCASE_GPU=1`（独立 640×480 展示窗 + Vista/DX12）。
GPU **永远显示直到关掉展示窗**（忽略残留的正数 `LINGER_MS`）。自动化用
`ATMOSPHERE_SHOWCASE_TIMED_MS=1500`，或 `ATMOSPHERE_SHOWCASE_LINGER_MS=0` 跳过停留。

```bat
set ATMOSPHERE_SHOWCASE_GPU=1
rem automation only: set ATMOSPHERE_SHOWCASE_TIMED_MS=1500
out\SmartGIS.exe --atmosphere-showcase=full
```

| 模式 | 行为 |
| --- | --- |
| `land` | 大气关，仅 DEM / land `present_gpu` |
| `ocean` | procedural 场 + 海洋开、云关 |
| `full` | `enable_atmosphere_demo()`（海+云） |
| `coast` | 东海附近 extent + full demo |

成功：exit 0；旁路 `out\Debug\captures\atmosphere-showcase-mark.txt` 与
`out\Debug\captures\atmosphere-showcase-<mode>.bmp`（GPU 要求 BMP 有可见像素信号）。
失败码：50 HWND、51 非 Vista、52 present、53 开关/场状态不符、54 BMP 全黑/无信号。

说明：showcase 启动前会 `set_scene3d_engine(kGdi)`，避免
`BrowserView::init` 多视口 Vista 挂起；GPU 绘制走独立 640×480 present HWND。
GPU BMP 需至少 2 种可见色（拒绝纯 clear）。根因修复：透视投影改为 RH，与 look_at（看向 -Z）一致。

2D 地图 carto showcase（MapLibre / Baidu 色板）。打开 China 样例、`export_bmp`
写旁路 `out\Debug\captures\map2d-showcase-china.bmp`。自动化：`MAP2D_SHOWCASE_LINGER_MS=0`
（当前无 linger；预留）。可选 `MAP2D_SHOWCASE_GPU=1` 额外跑 `present_gpu`。

**Align 模式**（长期 Style 对齐，不链 Native）：与 china 模式相同打开
`china_city` 样例，再加载 `maplibre/example/style_align.json`，同 mainland
视野出 `map2d-showcase-align.bmp`。对照脚本：

```bat
out\SmartGIS.exe --map2d-showcase=align
python testing\tools\harness\_shared\case\align\maplibre_align.py
```

```bat
out\SmartGIS.exe --map2d-showcase=china
python testing\tools\harness\map2d\map2d.china\map2d_china_loop.py --no-build
```

失败码：54 BMP 无信号、55 样例打开失败、56 导出失败、57 presenter 缺失。

```bat
build.bat views
out\SmartGIS.exe
out\SmartGIS.exe --self-test
py -3 testing\tools\loop_runner.py --suite browser.map2d.browse --no-build
py -3 testing\tools\loop_runner.py --list
```

Harness suites：契约在 `testing/tools/harness/<family>/<suite_id>/suite.json`，
专属 script/`*_loop.py` 与 JSON 同目录；跨 suite 工具在 `harness/_shared/`。与
`app/startup/scenario` id 对齐。详见
[`docs/superpowers/ui-testing.md`](../../../docs/superpowers/ui-testing.md) L1′。
Horizon PaintCounters matrix：`py -3 testing/tools/harness/ui/run_ui_profile_matrix.py`（skill `harness-auto-ui-opt`）。

样例也可直接 Open：`out\views_ogr_sample.geojson`（构建后可从
`testing/data/` 复制）或仓库内 `testing/data/views_ogr_sample.geojson`。

产出 `out/SmartGIS.exe`（`build_views=true`）。不在
`group("all")` 里。分层与退出码：
[`docs/superpowers/ui-testing.md`](../../../docs/superpowers/ui-testing.md)。

---

菜单 **Engine: Vista/DX12 / Stereo/GL / GDI** 发 `view.engine.*`，经
`content::set_scene3d_engine` 切换 3D 呈现后端并 reattach Scene3d 视口。
菜单 **RHI** / **MapLibre** 发 `view.backend.rhi` / `view.backend.maplibre`，经
`MapContents::SetRenderBackend` 通知 `--type=gpu` 切换 direct / tile
（`maplibre` 为 tile 的历史别名，非 MapLibre Native；热切换，不重启 GPU
子进程）。CEF HTML 同命令 id（`ActivateTool` / `tool.command` topic）。

**最后更新：** 2026-10-06
