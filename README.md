<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS

地图挂起来的时候，人只能看。我们要的不是这个。一张图该是你能走进去干活的地方。

2010 年前后，这事落在一间学生实验室里。打开数据，描一条线，再走进地形——人站在地上，而不是对着图纸发呆。他们把树做成插件的形状，不是赶时髦，是怕一张还能干活的图被一扇窗堵死。下一双手要接得进来。它叫 SmartGIS。

这些年留下的不是一份更全的功能册。留下的是脾气。老窗口还占着院子，不是为了陈列当年能凑什么，是因为那张图还没说完。有人还欠场景终于肯转过来的那一下。

所以才重写。不是要把实验室抹掉，也不是要把地形和点云再清点一遍。旧栈太倔。换手，只为让那张还能干活的图活下去。树还在长，新旧叠在同一棵树上。这不是陈列柜，是一份一起住过的代码还没说完的话。

同一份故事和目录在 [`docs/README.md`](docs/README.md)；树怎么分，在 [`docs/superpowers/src-layout.md`](docs/superpowers/src-layout.md)。**官方产品壳是 `SmartGIS.exe`**（Views + Skia，`build.bat app` / `views`）。`src/legacy/` 已删除，不要再加回来。构建产物在 **`out/Debug`** / **`out/Release`**（`build.bat` 默认两套；`build.bat debug|release` 只编一套），见 [`build/README.md`](build/README.md)。编译锁默认关；不要给 agent 开 `out/.build.lock.on`。

mogu 对齐 foundation 真源仅在 [`src/base/`](src/base/)（`//src/base:foundation`，多为 header-only / 静态聚合，**不是**产品 DLL；兼容别名 `//:base` / `//:core` 在根 `BUILD.gn`）。仓库根**无**物理 `base/`、`core/` 目录。GIS / UI / render / vista 等产品代码同在 [`src/`](src/)。产品平台 DLL 含 **`vista.dll` / `vista_d.dll`**（GN `//src/vista:vista`，`dll_stem=vista`；树 [`src/vista/`](src/vista/README.md)）。产品平台 DLL 另有 **`base.dll` / `base_d.dll`**（GN 标签 `//src/base:base`，`dll_stem=base`）。场景数学在 `src/base/math`（`//src/base/math:math`、`:bounds`，C++ 命名空间仍为 `render`），不链进 `base.dll`。设计见 [`docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`](docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md)。

---

## 产品壳

**`SmartGIS.exe` 就是 Views 产品**（`//src/app/views:views`）。没有 MFC leftover 壳、没有 `SmartGIS-Legacy.exe`。

| | 产品（出货） |
| --- | --- |
| 壳 | Chromium-style **Views**（widget / layout / events）+ **Skia**（canvas，不是 widget kit） |
| 树 | [`src/app/views/`](src/app/views/README.md) + [`src/ui/views`](src/ui/views/README.md) + [`src/ui/gfx`](src/ui/gfx/README.md) |
| 二进制 | `out/<config>/SmartGIS.exe` |
| 构建 | `build.bat debug app` 或 `build.bat debug views`（`build_views`） |
| 运行 | `out\Debug\SmartGIS.exe`；`--self-test` / `--map2d-showcase=china` / `--plugin-showcase=world3d` |
| UI | `ui_views` + `plugin_host`（Views builtin） |

共用车道：`gis`（模型 / OGR / style / tile）、`content`（MapSession / ViewHost / present）、`render`（Vista RHI）、`vista`（MapFrame / GpuScene）、`plugin` 产品包、`scenic`（探索性引擎，不在 `src_all`）、`gpu`（`--type=gpu` 同 PE）。地图仍是 HWND 视口，不是 Qt / WinUI / WebView2。

品牌对外仍可称 **SmartGIS Horizon**；窗口标题可以是 `SmartGIS Views`，与 PE 名 `SmartGIS.exe` 分开。Win32 类名 `SmartGisViewsWidget` 未随映像改名。

```bat
build.bat debug views
out\Debug\SmartGIS.exe
out\Debug\SmartGIS.exe --self-test
```

---

**最后更新：** 2026-10-04
