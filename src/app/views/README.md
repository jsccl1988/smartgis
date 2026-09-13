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
`group("all")` 里。`--self-test` 泵消息、检查 widget HWND，并在
`kContentMapView` 时调用 `MapViewport::wait_ready`。

Open：有 `MapContents` 则 `CatalogCall` 打开路径，并 `ViewHost::execute`
已有命令；否则只把路径写到状态栏。

---

**最后更新：** 2026-09-13
