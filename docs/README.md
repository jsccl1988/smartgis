<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS 文档

工程管理对齐 mogu：短英文文件名、`docs/build/`、入口只有 **GN**。根 README 讲怎么编；这里是文档索引。

mogu 源树本机未检出（常见路径 `c:\Dev\src\mogu`、WSL `/home/ccl/dev/src/mogu`）。布局按 mogu-mapping + mgis 的 mogu 式 README/`build/README.md` 对齐，不搬 Bazel。

## 索引

| 文档 | 内容 |
| --- | --- |
| [根 `README.md`](../README.md) | 怎么编、根目标组、`src/` 对照表 |
| [`build/README.md`](../build/README.md) | GN/Ninja toolchain |
| [`testing/README.md`](../testing/README.md) | 单测 / `build.bat e2e` 产品 exe 冒烟 |
| [`src/README.md`](../src/README.md) | 产品树分层（短名） |
| [`build/mogu-mapping.md`](build/mogu-mapping.md) | mogu → 本仓工程管理对照 |
| [`build/src-layout.md`](build/src-layout.md) | `src/` 分层 + 2010→短名表 |
| [`build/ui-views-skia.md`](build/ui-views-skia.md) | 桌面 UI 终局：Views + Skia |
| [`build/ui-shell-multiprocess.md`](build/ui-shell-multiprocess.md) | 可替换 chrome + 多进程渲染 |
| [`superpowers/specs/2026-09-13-ogr-db-datasource-design.md`](superpowers/specs/2026-09-13-ogr-db-datasource-design.md) | 用 GDAL/OGR 替换 ADO 数据库数据源（PostGIS / GeoPackage） |
| [`superpowers/specs/2026-09-13-gdal-layer-management-design.md`](superpowers/specs/2026-09-13-gdal-layer-management-design.md) | 全部图层管理走 GDAL Dataset/Layer（文件 / 库 / 内存适配器） |
| [`superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`](superpowers/specs/2026-09-13-algorithm-layer-oss-design.md) | 算法层：gdal_sdk GEOS + PROJ 9，合并 `SmtGeoCore`，DEM/chart 移出 algorithm |
| [`superpowers/specs/2026-09-13-base-ipc-mojom-design.md`](superpowers/specs/2026-09-13-base-ipc-mojom-design.md) | 单二进制 `--type=` + 独立 GPU 进程（2D/3D）+ Mojo/mojom |
| [`superpowers/specs/2026-09-13-render-rhi-scene-design.md`](superpowers/specs/2026-09-13-render-rhi-scene-design.md) | 统一 RHI（FlyCube DX12/Vulkan）+ `sdb` 模型/场景 + GPU scene |
| [`superpowers/specs/2026-09-13-net-asio-httplib-design.md`](superpowers/specs/2026-09-13-net-asio-httplib-design.md) | `src/net`：standalone ASIO + cpp-httplib + FnRPC 客户端 |
| [`superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](superpowers/specs/2026-09-13-tool-event-dispatch-design.md) | 工具层：session Command / Input / EventBus，与文档操作解耦 |
| [`superpowers/specs/2026-09-13-plugin-host-design.md`](superpowers/specs/2026-09-13-plugin-host-design.md) | 插件层：PluginHost / Registry / Views / Python / store |
| [`superpowers/plans/2026-09-13-net-asio-httplib.md`](superpowers/plans/2026-09-13-net-asio-httplib.md) | 实现计划：换掉 Winsock 1.1 / WebAppLib |
| [`superpowers/plans/2026-09-13-base-ipc-mojom.md`](superpowers/plans/2026-09-13-base-ipc-mojom.md) | 实现计划：C++23、ContentMain、同 PE 子进程 |
| [`superpowers/plans/2026-09-13-tool-event-dispatch.md`](superpowers/plans/2026-09-13-tool-event-dispatch.md) | 实现计划：CommandDispatcher + InteractionStack + EditSession |
| [`superpowers/plans/2026-09-13-plugin-host.md`](superpowers/plans/2026-09-13-plugin-host.md) | 实现计划：扩展平台（含 leftover `*.am` 适配） |

没有第二份 `doc/` 目录。2010 的 `readme.txt` / `说明.docx` 已并入本节「产品概要」。

## 工程入口

本仓只有 **GN**（`build.bat` → `gn gen out` + `ninja`）。mogu 的双轨是 GN + Bazel；这里**不搬 Bazel**，也不把 sln 当第二主轨。

| 入口 | 状态 |
| --- | --- |
| `build.bat` → `gn gen` + `ninja` | **唯一工程入口** |
| 已删除的 `vs2008/` / `branches/` | 不再存在；`build.bat sln` **拒绝** |

默认 `//:all` = `//src:src_all`（不依赖 MFC / D3DX9 的已接线 DLL）。主程序走 `build.bat app` / `views` / `web` / `winui` / `render`，不在日常 `group("all")` 里。

## 产品概要

系统按五层拆：`app`（产品壳）、`content`（稳定 API）、`sdb`（要素/图层/地图文档/数据源；图层开闭与要素 I/O 经 GDAL Dataset/Layer）、`render`（2D+3D + RHI）、`base`（原 core + 包络/样式）。WebGIS 在 `src/web/`。终局桌面壳是 Views + Skia。

3D/2D 地图 GPU 走 `src/render/rhi`（FlyCube DX12/Vulkan Facade）。逻辑模型与 World 在 `src/sdb/{model,scene}`；GPU 实例缓存在 `src/render/scene`。遗留 `scene3d` / `model3d` / GL 设备仍在 `src_all`。Web 地图发布：瓦片、WMS/WTS；源码在 `src/web/`。核心地图文档在 `src/sdb/map`（`SmtMap`）。

---

**最后更新：** 2026-09-13
