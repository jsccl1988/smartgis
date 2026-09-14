<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS

地图挂起来的时候，人只能看。我们要的不是这个。一张图该是你能走进去干活的地方。

2010 年前后，这事落在一间学生实验室里。打开数据，描一条线，再走进地形——人站在地上，而不是对着图纸发呆。他们把树做成插件的形状，不是赶时髦，是怕一张还能干活的图被一扇窗堵死。下一双手要接得进来。它叫 SmartGIS。

这些年留下的不是一份更全的功能册。留下的是脾气。老窗口还占着院子，不是为了陈列当年能凑什么，是因为那张图还没说完。有人还欠场景终于肯转过来的那一下。

所以才重写。不是要把实验室抹掉，也不是要把地形和点云再清点一遍。旧栈太倔。换手，只为让那张还能干活的图活下去。树还在长，新旧叠在同一棵树上。这不是陈列柜，是一份一起住过的代码还没说完的话。

同一份故事和目录在 [`docs/README.md`](docs/README.md)；树怎么分，在 [`docs/build/src-layout.md`](docs/build/src-layout.md)。

mogu 对齐 foundation 真源仅在 [`src/base/`](src/base/)（`//src/base:foundation`，多为 header-only / 静态聚合，**不是**产品 DLL；兼容别名 `//:base` / `//core:core` 在根 `BUILD.gn` / `core/BUILD.gn`）。仓库根**无**物理 `base/` 目录。GIS / UI / render 等产品代码同在 [`src/`](src/)。产品平台 DLL 为 **`platform.dll` / `platform_d.dll`**（GN 标签 `//src/base:base`）。设计见 [`docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`](docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md)。

---

**最后更新：** 2026-09-15
