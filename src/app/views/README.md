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
    MenuBar          Open / Exit / Map Edit / Datasource / 3D
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
图层名与几何灌进 Catalog / 2D overlay；打不开时才回退样例要素。样例数据：

- 仓库：`testing/data/views_ogr_sample.geojson`
- 自测：exe 旁写入 `views_ogr_selftest.geojson`

菜单 **Open** 或 Catalog「加载 shp」选上述文件即可；状态栏显示 `Opened (OGR): …`。

3D 页：`view3d.trackball` 更新 `Scene3dController` 的 yaw/pitch/distance，经
`make_orbit_camera` 喂给 FlyCube `present_gpu`（实心立方体）。默认挂接仍优先
`content::MapWidgetHostView`（`--self-test` 稳）；要强制 3D 走 FlyCube：

```bat
set SMT_PREFER_FLYCUBE_3D=1
out\SmartGisViews.exe
```

无 GPU / 初始化失败时仍为 GDI 线框兜底。`--self-test` 断言 OGR 进层与
相机矩阵变化；若本机挂上 FlyCube 会写 `flycube-camera-ok`。

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

**最后更新：** 2026-09-14
