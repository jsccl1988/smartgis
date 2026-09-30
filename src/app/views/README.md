<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` — Scheme 3 shell

Product shell for **Views + Skia**。`SmartGisViews.exe` 是宿主：`Widget` +
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
        Splitter horizontal
          CatalogView (~240)
          TabStrip (flex): Map | Data | 3D
            各页 MapViewport（非活动 HWND 隐藏）
        AmboxView (~200)
      TabStrip inspector: FeatureInfo | AttributeTable | …
      DebugConsolePanel（底栏 Diagnostic Tools，默认折叠；View → Toggle Diagnostic Tools）
        tabs: Output | Console | CPU | Memory
    StatusBar
```

Debug Console / LogSink / Agent / Python worker：见
[`docs/superpowers/specs/2026-09-28-debug-console-design.md`](../../docs/superpowers/specs/2026-09-28-debug-console-design.md)。
启用：`--debug-console` / `SG_DEBUG=1` / 菜单 Toggle。
三个地图页各自一个 `MapViewport` + `content::ViewHost`（2D 编辑 / 2D 浏览 /
3D）。`MapContents` 会话共享；`OpenView` 分别为 `kMapEdit` / `kMapData` /
`kScene3d`。3D 若无法挂接则保持 native 占位，鼠标不崩。

源码按职责分目录（无根目录转发头）。Chromium 分层见 living shell spec
**§Content sink**：`app/views` 只留 `shell/`（≈ chrome）。`Browser` 持有
`content::MapSession`（≈ WebContents：拥有 `MapScene` / camera / present /
gestures / ViewHosts / `MapContents*`）；能力实现在
`src/content/browser/{document,camera,present,input}`；GDI paint 在
`content/browser/present/*/paint/`。`shell/ui` → `shell/browser` →
`//src/content:map_session`；**禁止** `present` → `shell`。`shell/`：`app/`、
`browser/`、`ui/`、`harness/{showcase,self_test}/`。`main.cc` 仅 `wWinMain` 胶水。
Present README：
[`../../content/browser/present/README.md`](../../content/browser/present/README.md)。

`wWinMain` → CLI11 解析 → `content::content_main`（`process_type_set`），
再进 `browser_main` / `gpu_main` / `renderer_main`。同一 PE 以 `--type=gpu`
/ `--type=renderer` 再拉起。地图挂接仍走 `MapViewport::attach()`；原生 HWND
把鼠标 / 键 / 滚轮转给 `ViewHost::dispatch_input`。

```bat
build.bat views
```

产出 `out/SmartGisViews.exe`（`smt_build_views=true`）。不在
`group("all")` 里。`--self-test` 泵消息、检查 widget HWND，切换 Map/Data/3D
页，在 `kContentMapView` 时 `wait_ready`，并对 3D 页跑 `view3d.trackball`
输入（无 GPU 时占位 HWND 亦可）。分层与退出码：
[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

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

**2D 主路径 = RHI**：Map/Data 页默认 FlyCube；`MapScene::present_gpu` 把可见矢量层交给 `gis::vista::Layout` 生成 `MapFrame`，再由 `effect::map::Pass` 录到调用方 `Device` 并 present。成功时注记在帧内（`kText`），`paint_annotation_overlay` 只描选中；失败或强制时回退全量 GDI `MapScene::paint`（含注记）。

```bat
rem 强制 2D 走 ContentMapView / 跳过 FlyCube：
set SMT_FORCE_CONTENT_MAPVIEW_2D=1
rem 或: set SMT_PREFER_FLYCUBE_2D=0
rem 强制 GDI 全量 overlay（仍可挂 FlyCube HWND，但不走 present_gpu）：
set SMT_FORCE_GDI_MAP_OVERLAY=1
out\SmartGisViews.exe
```

3D 页：`view3d.trackball` 更新 `OrbitFrame` / `Scene3dPresenter`。默认
**FlyCube RHI**（`present_gpu`；成功时 shell 只叠 `paint_hud`）。挂接失败时
回退 ContentMapView / GDI `Scene3dPresenter::paint()`。**不会**在 FlyCube
SoT 下再挂 leftover OpenGL（同 HWND 抢 swapchain 会把徽章永久钉成
`Stereo/GL`）。

**手动切换 3D 引擎**（不经环境变量）：菜单 **View → Engine: FlyCube/DX12 /
Stereo/GL / GDI**，或 `content::set_scene3d_engine(...)`。命令 id：
`view.engine.flycube` / `view.engine.stereo_gl` / `view.engine.gdi`。切换时
会 detach/reattach Scene3d `MapViewport`，并按选择挂放 stereo。

3D HUD 显示引擎名；画面**右下角**有引擎 Logo 徽章（与真实后端一致：
`FlyCube/DX12` / `Stereo/GL` / `GDI` / `ContentMapView` / `Null`）。DEM 默认
叠 hypsometric 着色；若存在 `china_rs.tif` / `china_imagery.tif`（exe 旁或
`testing/data/`）则 draping 遥感影像。TIN 线框：

```bat
set SMT_SCENE3D_WIREFRAME=1
out\SmartGisViews.exe
```

`--self-test` 会 `set_scene3d_engine(kGdi)`（挂起规避），并断言 OGR 进层与相机矩阵；若挂上
FlyCube 会写 `flycube-camera-ok`，并在 present 前开 `enable_atmosphere_demo()`。

大气 3D 端到端 showcase。默认 **Null RHI**（可重复退出 0）；
真 GPU：`set SMT_ATMOSPHERE_SHOWCASE_GPU=1`（独立 640×480 展示窗 + FlyCube/DX12）。
GPU **永远显示直到关掉展示窗**（忽略残留的正数 `LINGER_MS`）。自动化用
`SMT_ATMOSPHERE_SHOWCASE_TIMED_MS=1500`，或 `SMT_ATMOSPHERE_SHOWCASE_LINGER_MS=0` 跳过停留。

```bat
set SMT_ATMOSPHERE_SHOWCASE_GPU=1
rem automation only: set SMT_ATMOSPHERE_SHOWCASE_TIMED_MS=1500
out\SmartGisViews.exe --atmosphere-showcase=full
```

| 模式 | 行为 |
| --- | --- |
| `land` | 大气关，仅 DEM / land `present_gpu` |
| `ocean` | procedural 场 + 海洋开、云关 |
| `full` | `enable_atmosphere_demo()`（海+云） |
| `coast` | 东海附近 extent + full demo |

成功：exit 0；旁路 `out\atmosphere-showcase-mark.txt` 与
`out\atmosphere-showcase-<mode>.bmp`（GPU 要求 BMP 有可见像素信号）。
失败码：50 HWND、51 非 FlyCube、52 present、53 开关/场状态不符、54 BMP 全黑/无信号。

说明：showcase 启动前会 `set_scene3d_engine(kGdi)`，避免
`BrowserView::init` 多视口 FlyCube 挂起；GPU 绘制走独立 640×480 present HWND。
GPU BMP 需至少 2 种可见色（拒绝纯 clear）。根因修复：透视投影改为 RH，与 look_at（看向 -Z）一致。

2D 地图 carto showcase（MapLibre / Baidu 色板）。打开 China 样例、`export_bmp`
写旁路 `out\map2d-showcase-china.bmp`。自动化：`SMT_MAP2D_SHOWCASE_LINGER_MS=0`
（当前无 linger；预留）。可选 `SMT_MAP2D_SHOWCASE_GPU=1` 额外跑 `present_gpu`。

**Align 模式**（长期 Style 对齐，不链 Native）：与 china 模式相同打开
`china_city` 样例，再加载 `maplibre/example/style_align.json`，同 mainland
视野出 `map2d-showcase-align.bmp`。对照脚本：

```bat
out\SmartGisViews.exe --map2d-showcase=align
python testing\tools\case\maplibre_align.py
```

```bat
out\SmartGisViews.exe --map2d-showcase=china
python testing\tools\case\map2d_shot_loop.py --no-build
```

失败码：54 BMP 无信号、55 样例打开失败、56 导出失败、57 presenter 缺失。

```bat
build.bat views
out\SmartGisViews.exe
out\SmartGisViews.exe --self-test
py -3 testing\tools\loop_runner.py --suite browse --no-build
py -3 testing\tools\loop_runner.py --list
```

Harness suites：契约在 `testing/tools/suites/*.json`，case 脚本在
`testing/tools/case/`，与 `shell/harness/scenario_registry` id 对齐。详见
[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md) L1′。

样例也可直接 Open：`out\views_ogr_sample.geojson`（构建后可从
`testing/data/` 复制）或仓库内 `testing/data/views_ogr_sample.geojson`。

产出 `out/SmartGisViews.exe`（`smt_build_views=true`）。不在
`group("all")` 里。分层与退出码：
[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

---

菜单 **Engine: FlyCube/DX12 / Stereo/GL / GDI** 发 `view.engine.*`，经
`content::set_scene3d_engine` 切换 3D 呈现后端并 reattach Scene3d 视口。
菜单 **RHI** / **MapLibre** 发 `view.backend.rhi` / `view.backend.maplibre`，经
`MapContents::SetRenderBackend` 通知 `--type=gpu` 切换 direct / tile
（`maplibre` 为 tile 的历史别名，非 MapLibre Native；热切换，不重启 GPU
子进程）。CEF HTML 同命令 id（`ActivateTool` / `tool.command` topic）。

**最后更新：** 2026-09-29
