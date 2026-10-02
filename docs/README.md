<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS 文档

根上的 README 是第一站。这里同一份故事，并留下目录，方便从那张图走到具体的文件。

工程上的对齐——短文件名、`docs/build/`、入口只有 `build.bat`——仍然有效。mogu 源树本机未检出也没关系。不搬 Bazel。

## 本地预览（Portal）

一键浏览本目录下全部 Markdown + HTML（侧栏目录、MD 渲染、设计原理图整页嵌入）：

```bat
docs\portal\open.bat
REM 或: py -3 docs\portal\serve.py
```

说明见 [`portal/README.md`](portal/README.md)。

## 索引

### 产品 / 构建（as-built）

| 文档 | 内容 |
| --- | --- |
| [根 `README.md`](../README.md) | 产品故事 |
| [`build/README.md`](../build/README.md) | GN/Ninja toolchain |
| [`testing/README.md`](../testing/README.md) | 单测 / `build.bat e2e` 产品 exe 冒烟 |
| [`src/README.md`](../src/README.md) | 产品树分层（短名） |
| [`build/mogu-mapping.md`](build/mogu-mapping.md) | mogu → 本仓工程管理对照 |
| [`build/src-layout.md`](build/src-layout.md) | `src/` 分层 + 2010→短名表；foundation 在 `src/base` |
| [`../src/base/README.md`](../src/base/README.md) | foundation + 产品平台 DLL |
| [`build/abi-rename-map.md`](build/abi-rename-map.md) | include / dll_stem / 导出宏 |
| [`build/ui-views-skia.md`](build/ui-views-skia.md) | 桌面 UI 终局：Views + Skia |
| [`build/ui-testing.md`](build/ui-testing.md) | GUI / Views 测试分层（L0–L4） |
| [`build/gis-test-matrix.md`](build/gis-test-matrix.md) | `src/gis` 功能矩阵 + 覆盖率/基准入口 |
| [`build/ui-shell-multiprocess.md`](build/ui-shell-multiprocess.md) | 可替换 chrome + 多进程渲染 |
| [`build/views-window-process.html`](build/views-window-process.html) | Views 窗口体系 / 进程体系（启动·运行·关闭） |

### Superpowers（in-flight）

**默认改 living 伞的 `§`，禁止轻易开新 dated topic。** 仅 9 行 Active 表：[`superpowers/README.md`](superpowers/README.md)。规则：`.cursor/rules/repo/superpowers-docs.mdc`。

| Living 真源 | 内容 |
| --- | --- |
| [`superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) | Leftover SP0–SP5 |
| [`superpowers/specs/2026-09-13-render-rhi-scene-design.md`](superpowers/specs/2026-09-13-render-rhi-scene-design.md) | RHI + 双场景 + map2d + atmosphere + model/compute |
| [`superpowers/specs/2026-09-27-views-desktop-shell-design.md`](superpowers/specs/2026-09-27-views-desktop-shell-design.md) | `app/views` 壳 + toolkit + markup + panels + debug console |
| [`superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](superpowers/specs/2026-09-13-tool-event-dispatch-design.md) | Tool dispatch + `src/tool` |
| [`superpowers/specs/2026-09-14-base-root-hybrid-design.md`](superpowers/specs/2026-09-14-base-root-hybrid-design.md) | `src/base` (+ memory / PA-E / execution / codecs) |
| [`superpowers/specs/2026-09-13-gdal-layer-management-design.md`](superpowers/specs/2026-09-13-gdal-layer-management-design.md) | GIS datasource / GDAL / SDB / Session+Provider |
| [`superpowers/specs/2026-09-13-plugin-host-design.md`](superpowers/specs/2026-09-13-plugin-host-design.md) | Plugin host / contributions / store |
| [`superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`](superpowers/specs/2026-09-13-algorithm-layer-oss-design.md) | Algorithm layer (OSS) |
| [`superpowers/specs/2026-09-13-net-asio-httplib-design.md`](superpowers/specs/2026-09-13-net-asio-httplib-design.md) | Net (asio / httplib) |
| [`superpowers/archive/`](superpowers/archive/) | 已落地 / 废止 / merge-B 子 topic |

实现勾选见各伞挂靠的 `superpowers/plans/`；不要为已有伞再开平行 design。

没有第二份 `doc/` 目录。2010 的说明已经并进故事里；不要把那份 sln、那条 Web 发布当现状。

## 这张图

地图挂起来的时候，人只能看。我们要的不是这个。一张图该是你能走进去干活的地方。

2010 年前后，这事落在一间学生实验室里。打开数据，描一条线，再走进地形——人站在地上，而不是对着图纸发呆。他们把树做成插件的形状，不是赶时髦，是怕一张还能干活的图被一扇窗堵死。下一双手要接得进来。它叫 SmartGIS。

这些年留下的不是一份更全的功能册。留下的是脾气。老窗口还占着院子，不是为了陈列当年能凑什么，是因为那张图还没说完。有人还欠场景终于肯转过来的那一下。

所以才重写。不是要把实验室抹掉，也不是要把地形和点云再清点一遍。旧栈太倔。换手，只为让那张还能干活的图活下去。树还在长，新旧叠在同一棵树上。这不是陈列柜，是一份一起住过的代码还没说完的话。

目录怎么分，见 [`build/src-layout.md`](build/src-layout.md)。模型、场景，见 [`superpowers/specs/2026-09-13-render-rhi-scene-design.md`](superpowers/specs/2026-09-13-render-rhi-scene-design.md)。

---

**最后更新：** 2026-10-02
