<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS 文档

根上的 README 是第一站。这里同一份故事，并留下目录，方便从那张图走到具体的文件。

工程上的对齐——短文件名、`docs/build/`、入口只有 `build.bat`——仍然有效。mogu 源树本机未检出也没关系。不搬 Bazel。

## 索引

| 文档 | 内容 |
| --- | --- |
| [根 `README.md`](../README.md) | 产品故事 |
| [`build/README.md`](../build/README.md) | GN/Ninja toolchain |
| [`testing/README.md`](../testing/README.md) | 单测 / `build.bat e2e` 产品 exe 冒烟 |
| [`src/README.md`](../src/README.md) | 产品树分层（短名） |
| [`build/mogu-mapping.md`](build/mogu-mapping.md) | mogu → 本仓工程管理对照 |
| [`build/src-layout.md`](build/src-layout.md) | `src/` 分层 + 2010→短名表；foundation 在 `src/base` |
| [`../src/base/README.md`](../src/base/README.md) | foundation（`//src/base:foundation`）+ 产品平台 DLL（`dll_stem=platform`）；兼容别名 `//:base` / `//core:core` |
| [`build/abi-rename-map.md`](build/abi-rename-map.md) | include / dll_stem / 导出宏；含 DLL reorg 终态（一层一 DLL + `_d`）；产品平台 stem 终局 `platform` |
| [`superpowers/specs/2026-09-14-base-root-hybrid-design.md`](superpowers/specs/2026-09-14-base-root-hybrid-design.md) | foundation Hybrid（已收口；真源现为 `src/base`） |
| [`build/ui-views-skia.md`](build/ui-views-skia.md) | 桌面 UI 终局：Views + Skia |
| [`build/ui-testing.md`](build/ui-testing.md) | GUI / Views 测试分层（L0–L4）与门禁 |
| [`build/ui-shell-multiprocess.md`](build/ui-shell-multiprocess.md) | 可替换 chrome + 多进程渲染 |
| [`superpowers/specs/2026-09-14-app-cef-hwnd-host-design.md`](superpowers/specs/2026-09-14-app-cef-hwnd-host-design.md) | CEF 第三壳：分区 HWND + 原生地图（`SmartGisCef.exe`；默认不编） |
| [`superpowers/specs/2026-09-15-app-cs-winui-host-design.md`](superpowers/specs/2026-09-15-app-cs-winui-host-design.md) | 外部 C# WinUI：`MapView` 控件 + `SmartGisCs.exe`（默认不编） |
| [`superpowers/plans/2026-09-14-app-cef-hwnd-host.md`](superpowers/plans/2026-09-14-app-cef-hwnd-host.md) | 实现计划：CEF pin + 分区 HWND + ChromeBridge + `--self-test` |
| [`superpowers/specs/2026-09-13-ogr-db-datasource-design.md`](superpowers/specs/2026-09-13-ogr-db-datasource-design.md) | 用 GDAL/OGR 替换 ADO 数据库数据源（PostGIS / GeoPackage） |
| [`superpowers/specs/2026-09-13-gdal-layer-management-design.md`](superpowers/specs/2026-09-13-gdal-layer-management-design.md) | 全部图层管理走 GDAL Dataset/Layer（文件 / 库 / 内存适配器） |
| [`superpowers/specs/2026-09-13-tile-layer-provider-design.md`](superpowers/specs/2026-09-13-tile-layer-provider-design.md) | 2D 地图瓦片：TileProvider（HTTP(S) XYZ/WMTS + 磁盘缓存 + Views 底图对话框）；不进 OGR / `SDBD:MEM` |
| [`superpowers/specs/2026-09-18-china-city-map-plpt-design.md`](superpowers/specs/2026-09-18-china-city-map-plpt-design.md) | 中国地级离线底图：区/线/点/注记（`china_city.gpkg` + MapScene `kText`；高精度） |
| [`superpowers/specs/2026-09-14-sdb-style-document-design.md`](superpowers/specs/2026-09-14-sdb-style-document-design.md) | 制图样式：Style JSON + 符号库 + 规则引擎（`sdb/style`；POD 在 `sdb/carto`） |
| [`superpowers/plans/2026-09-14-sdb-style-document.md`](superpowers/plans/2026-09-14-sdb-style-document.md) | 实现计划：`sdb::style` 模块 + 单测 + MapLayer 挂接 |
| [`superpowers/specs/2026-09-13-sdb-feature-maplayer-composition-design.md`](superpowers/specs/2026-09-13-sdb-feature-maplayer-composition-design.md) | `Feature` / `MapLayer`：OGR + 组合（修正裸 OGR ABI） |
| [`superpowers/plans/2026-09-13-sdb-feature-maplayer-composition.md`](superpowers/plans/2026-09-13-sdb-feature-maplayer-composition.md) | 实现计划：组合类型 + `src_all` 大爆炸 |
| [`superpowers/specs/2026-09-13-model-render-compute-design.md`](superpowers/specs/2026-09-13-model-render-compute-design.md) | 模型 / 渲染 / 计算深度设计（OSS 优先；leftover `src/render/*` → 目标架构） |
| [`superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`](superpowers/specs/2026-09-13-algorithm-layer-oss-design.md) | 算法层：`//third_party:gdal` GEOS + PROJ 9，合并 `SmtGeoCore`，DEM/chart 移出 algorithm |
| [`superpowers/specs/2026-09-13-base-ipc-mojom-design.md`](superpowers/specs/2026-09-13-base-ipc-mojom-design.md) | 单二进制 `--type=` + 独立 GPU 进程（2D/3D）+ Mojo/mojom |
| [`superpowers/specs/2026-09-13-base-archive-design.md`](superpowers/specs/2026-09-13-base-archive-design.md) | A1：BinarySink/Serializer 上提 `base/archive`；`net::Pickle` 留在 `net/pack` |
| [`superpowers/specs/2026-09-13-render-rhi-scene-design.md`](superpowers/specs/2026-09-13-render-rhi-scene-design.md) | 统一 RHI（FlyCube DX12/Vulkan）+ `sdb` 模型/场景 + GPU scene |
| [`superpowers/specs/2026-09-13-net-asio-httplib-design.md`](superpowers/specs/2026-09-13-net-asio-httplib-design.md) | `src/net`：standalone ASIO + cpp-httplib + OpenSSL HTTPS + FnRPC 客户端 |
| [`superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](superpowers/specs/2026-09-13-tool-event-dispatch-design.md) | 工具层：session Command / Input / EventBus，与文档操作解耦 |
| [`superpowers/archive/specs/2026-09-13-tool-legacy-split-design.md`](superpowers/archive/specs/2026-09-13-tool-legacy-split-design.md) | leftover `SmtIATool` → `src/legacy/tool/`（已落地）；`tool/` 仅终局 dispatch |
| [`superpowers/archive/plans/2026-09-13-tool-legacy-split.md`](superpowers/archive/plans/2026-09-13-tool-legacy-split.md) | 实现计划（已落地）：物理平移 + include/GN 改名，无转发头 |
| [`superpowers/specs/2026-09-13-plugin-host-design.md`](superpowers/specs/2026-09-13-plugin-host-design.md) | 插件层：PluginHost / Registry / Views / Python / store |
| [`superpowers/specs/2026-09-14-plugin-full-upgrade-design.md`](superpowers/specs/2026-09-14-plugin-full-upgrade-design.md) | 插件全面升级：Views 切轨 → 退役 MFC 壳 → 风格/ABI → 域能力 |
| [`superpowers/specs/2026-09-14-plugin-subdir-layout-design.md`](superpowers/specs/2026-09-14-plugin-subdir-layout-design.md) | 插件子目录：`host/` + `legacy/<domain>/` + 产品域 |
| [`superpowers/specs/2026-09-14-dll-reorganization-design.md`](superpowers/specs/2026-09-14-dll-reorganization-design.md) | 动态库重组：一层一 DLL（手法 C）；Phase 1 已落地 |
| [`superpowers/plans/2026-09-14-dll-reorganization.md`](superpowers/plans/2026-09-14-dll-reorganization.md) | 实现计划：平台 DLL 合并 + 文档回写 |
| [`superpowers/plans/2026-09-14-plugin-full-upgrade.md`](superpowers/plans/2026-09-14-plugin-full-upgrade.md) | 实现计划：PluginChrome + DEM 无 MFC 内核 + 并行车道 |
| [`superpowers/specs/2026-09-13-ui-views-controls-design.md`](superpowers/specs/2026-09-13-ui-views-controls-design.md) | Views 公共工具箱 vs `src/app/views` 组合；控件 + GIS 面板 |
| [`superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md) | leftover MFC chrome → `ui::views`（`SmartGisViews.exe` 唯一入口；Splitter + tabs） |
| [`superpowers/specs/2026-09-14-render-skia-canvas-design.md`](superpowers/specs/2026-09-14-render-skia-canvas-design.md) | Views 壳画布：`render::skia` 边界；GDI stub → 条件准入真 Skia |
| [`superpowers/plans/2026-09-14-render-skia-canvas.md`](superpowers/plans/2026-09-14-render-skia-canvas.md) | 实现计划：最小 Canvas API + 测试 + 真 Skia 准入（不进 src_all） |
| [`superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md) | 全仓 mogu 式 include + snake_case/两层命名空间 + 破 `Smt*` ABI（大爆炸） |
| [`superpowers/plans/2026-09-13-code-style-include-abi-cutover.md`](superpowers/plans/2026-09-13-code-style-include-abi-cutover.md) | 实现计划：映射表 + 按树并行改写 + 收尾验收 |
| [`superpowers/plans/2026-09-13-net-asio-httplib.md`](superpowers/plans/2026-09-13-net-asio-httplib.md) | 实现计划：换掉 Winsock 1.1 / WebAppLib |
| [`superpowers/plans/2026-09-13-base-ipc-mojom.md`](superpowers/plans/2026-09-13-base-ipc-mojom.md) | 实现计划：C++23、ContentMain、同 PE 子进程 |
| [`superpowers/plans/2026-09-13-tool-event-dispatch.md`](superpowers/plans/2026-09-13-tool-event-dispatch.md) | 实现计划：CommandDispatcher + InteractionStack + EditSession |
| [`superpowers/plans/2026-09-13-plugin-host.md`](superpowers/plans/2026-09-13-plugin-host.md) | 实现计划：扩展平台（含 leftover `*.am` 适配） |
| [`superpowers/archive/`](superpowers/archive/) | 已落地 / 废止（含删除的 mapd / web 栈） |

没有第二份 `doc/` 目录。2010 的说明已经并进故事里；不要把那份 sln、那条 Web 发布当现状。

## 这张图

地图挂起来的时候，人只能看。我们要的不是这个。一张图该是你能走进去干活的地方。

2010 年前后，这事落在一间学生实验室里。打开数据，描一条线，再走进地形——人站在地上，而不是对着图纸发呆。他们把树做成插件的形状，不是赶时髦，是怕一张还能干活的图被一扇窗堵死。下一双手要接得进来。它叫 SmartGIS。

这些年留下的不是一份更全的功能册。留下的是脾气。老窗口还占着院子，不是为了陈列当年能凑什么，是因为那张图还没说完。有人还欠场景终于肯转过来的那一下。

所以才重写。不是要把实验室抹掉，也不是要把地形和点云再清点一遍。旧栈太倔。换手，只为让那张还能干活的图活下去。树还在长，新旧叠在同一棵树上。这不是陈列柜，是一份一起住过的代码还没说完的话。

目录怎么分，见 [`build/src-layout.md`](build/src-layout.md)。模型、场景，见 [`superpowers/specs/2026-09-13-model-render-compute-design.md`](superpowers/specs/2026-09-13-model-render-compute-design.md)。

---

**最后更新：** 2026-09-15
