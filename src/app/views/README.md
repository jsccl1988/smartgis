<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` — Scheme 3 chrome

Product shell for **Views + Skia**。`SmartGisViews.exe` 是宿主：`Widget` +
layout + 公开 `ui::views` 控件 + 命令接线。不手绘 catalog / feature / status。

单窗口 IDE 布局（非 MDI）：

```
ui::views::Widget
  RootView  BoxLayout vertical
    MenuBar          Open / Exit / Map / Data / 3D / Select / Draw / Clear /
                     Undo / RHI / MapLibre / Plugins
    Splitter vertical (flex)
      Splitter horizontal
        Splitter horizontal
          CatalogView (~240)
          TabStrip (flex): Map | Data | 3D
            各页 MapViewport（非活动 HWND 隐藏）
        AmboxView (~200)
      TabStrip inspector: FeatureInfo | AttributeTable
    StatusBar
```

三个地图页各自一个 `MapViewport` + `content::ViewHost`（2D 编辑 / 2D 浏览 /
3D）。`MapContents` 会话共享；`OpenView` 分别为 `kMapEdit` / `kMapData` /
`kScene3d`。3D 若无法挂接则保持 native 占位，鼠标不崩。

`wWinMain` → `content::ContentMain`，带 `browser_main`、`gpu_main`
（`gpu::GpuMain`）、`renderer_main`。同一 PE 以 `--type=gpu` /
`--type=renderer` 再拉起。地图挂接仍走 `MapViewport::attach()`；原生 HWND
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
`seed_default` / 自测优先加载 **`china_city.gpkg`**（地级四层：`area` /
`line` / `point` / `text`）；缺失时回退 `china_plp.geojson`。样例数据：

- 仓库：`testing/data/china_city.gpkg`（约 3.4MB；构建复制到 `out/`；同目录有匹配的 `china_city.geojson`）
- 生成：`py -3 testing/data/build_china_city.py`（DataV 地级界 + Natural Earth 河流）
- 许可 / PIN：`testing/data/china_city.LICENSE.txt`、`china_city.PIN.txt`
- 兜底：`testing/data/china_plp.geojson`
- 自测：优先 `out/china_city.gpkg` / `.geojson`（≥4 层或 kind 四分、要素量级远高于示意 PLP）

菜单 **Open** 或 Catalog「加载 shp」选上述文件即可；状态栏显示 `Opened (OGR): …`。
图层右键 **View** 缩放到全图。

地图页是 **共享场景宿主**：`MapContents::OpenView`（`kMapEdit` / `kMapData` /
`kScene3d`）+ `SetExtent`（有中国范围则全幅中国）。2D 为正交，3D 为透视。
手势：滚轮对光标缩放、平移；HWND 允许时双指捏合（`WM_GESTURE` / 指针）。

3D 页：`view3d.trackball` 更新 `Scene3dController`。默认 ContentMapView 时
GPU 对 `kScene3d` 发布合成 DEM 线框（`paint_demo_frame`）；chrome 在 **尚无共享
DIB** 时用 GDI `paint()`（`seed_china_dem_into_world`），有帧或 FlyCube 时只叠
`paint_hud()` + 矢量。FlyCube 着色 DEM：

```bat
set SMT_PREFER_FLYCUBE_3D=1
out\SmartGisViews.exe
```

无 GPU 时 GDI DEM 线框兜底。`--self-test` 断言 OGR 进层与相机矩阵；若挂上
FlyCube 会写 `flycube-camera-ok`，并在 present 前开 `enable_atmosphere_demo()`。

大气 3D 端到端 showcase（自动 present 后退出）。默认 **Null RHI**（可重复退出 0）；
真 GPU：`set SMT_ATMOSPHERE_SHOWCASE_GPU=1`（部分机器在 post-detach FlyCube
`present_gpu` 上会挂起，已在本机复现）。

```bat
set SMT_RUN_FLYCUBE_GPU=1
out\SmartGisViews.exe --atmosphere-showcase=land
out\SmartGisViews.exe --atmosphere-showcase=ocean
out\SmartGisViews.exe --atmosphere-showcase=full
out\SmartGisViews.exe --atmosphere-showcase=coast
```

| 模式 | 行为 |
| --- | --- |
| `land` | 大气关，仅 DEM / land `present_gpu` |
| `ocean` | procedural 场 + 海洋开、云关 |
| `full` | `enable_atmosphere_demo()`（海+云） |
| `coast` | 东海附近 extent + full demo |

成功：exit 0；旁路 `out\atmosphere-showcase-mark.txt` 与
`out\atmosphere-showcase-<mode>.bmp`。失败码：50 HWND、51 非 FlyCube、
52 present、53 开关/场状态不符。

```bat
build.bat views
out\SmartGisViews.exe
out\SmartGisViews.exe --self-test
```

样例也可直接 Open：`out\views_ogr_sample.geojson`（构建后可从
`testing/data/` 复制）或仓库内 `testing/data/views_ogr_sample.geojson`。

产出 `out/SmartGisViews.exe`（`smt_build_views=true`）。不在
`group("all")` 里。分层与退出码：
[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

---

菜单 **RHI** / **MapLibre** 发 `view.backend.rhi` / `view.backend.maplibre`，经
`MapContents::SetRenderBackend` 通知 `--type=gpu` 切换 Track B / Track A（热切换，
不重启 GPU 子进程）。CEF HTML 同命令 id（`ActivateTool` / `tool.command` topic）。

**最后更新：** 2026-09-19
