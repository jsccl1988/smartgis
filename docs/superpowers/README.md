<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Superpowers

Living specs, open plans, HTML diagrams, and as-built product notes live here. Per-module facts also stay in `src/**/README.md`.

**Default: revise the living row.** Do **not** open a new dated `YYYY-MM-DD-*-design.md` or extra plan unless the Gate in `.cursor/rules/repo/superpowers-docs.mdc` is satisfied (new top-level subsystem **and** a written “why § merge is impossible”). Every new requirement continues as a `§` on an existing umbrella below.

## As-built

| 文档 | 内容 |
| --- | --- |
| [`src-layout.md`](src-layout.md) | `src/` 分层 + 2010→短名表；foundation 在 `src/base` |
| [`abi-rename-map.md`](abi-rename-map.md) | include / dll_stem / 导出宏 |
| [`mogu-mapping.md`](mogu-mapping.md) | mogu → 本仓工程管理对照 |
| [`ui-views-skia.md`](ui-views-skia.md) | 桌面 UI 终局：Views + Skia |
| [`ui-testing.md`](ui-testing.md) | GUI / Views 测试分层（L0–L4） |
| [`gis-test-matrix.md`](gis-test-matrix.md) | `src/gis` 功能矩阵 + 覆盖率/基准入口 |
| [`ui-shell-multiprocess.md`](ui-shell-multiprocess.md) | 可替换 chrome + 多进程渲染 |
| [`industry-gap-matrix.md`](industry-gap-matrix.md) | 业界差距矩阵 |

### Diagrams

| HTML | 内容 |
| --- | --- |
| [`diagrams/gis-vista-architecture.html`](diagrams/gis-vista-architecture.html) | GIS 泳道 + LayerBatch→present（`gis` ↛ `rhi`） |
| [`diagrams/vista-subdirectory-layers.html`](diagrams/vista-subdirectory-layers.html) | `vista.dll` 责任层 + CPU/GPU 子目录（gis ↛ vista） |
| [`diagrams/gis-geo-layers.html`](diagrams/gis-geo-layers.html) | `gis/geo` Types / Traits / Ops / Codec / Present |
| [`diagrams/gis-model-ogr-layers.html`](diagrams/gis-model-ogr-layers.html) | `gis/{feature,layer,map}` 产品面 vs leftover ABI；Map/Layer/Feature ↔ OGR |
| [`diagrams/gis-algorithm-geometry-split.html`](diagrams/gis-algorithm-geometry-split.html) | 几何体 ⊥ 算法（补图） |
| [`diagrams/map2d-present-frame.html`](diagrams/map2d-present-frame.html) | Map2d MapFrame 四层 + 双出口 |
| [`diagrams/render-accelerate-topology.html`](diagrams/render-accelerate-topology.html) | GPU-process × vista parallel（A×B） |
| [`diagrams/ui-views-shell-architecture.html`](diagrams/ui-views-shell-architecture.html) | Views shell / compositor 泳道 |
| [`diagrams/content-browser-layers.html`](diagrams/content-browser-layers.html) | `content/browser` 责任层 + C11 收紧 + MapSession / content.dll |
| [`diagrams/views-window-process.html`](diagrams/views-window-process.html) | Views 窗口体系 / 进程体系 |
| [`diagrams/legacy-render-architecture.html`](diagrams/legacy-render-architecture.html) | **Scenic**（`src/scenic` 工作副本 + 冻结 leftover/render） |
| [`diagrams/debug-console-agent.html`](diagrams/debug-console-agent.html) | Debug console agent |
| [`diagrams/plugin-product-world3d.html`](diagrams/plugin-product-world3d.html) | world3d 包：DEM + 2D orthogrid + 3D hex |

Legacy UI/app deep-layer slices: `diagrams/legacy-{app,ui-widgets,ui-map,ui-shell,ui-inspect,ui-dialogs}-deep-layer.html`.

## Active living (only these)

| Topic | Living spec | Primary open plans |
| --- | --- | --- |
| Leftover strangler SP0–SP5 | [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) | [`plans/2026-09-19-tool-behavior-migration.md`](plans/2026-09-19-tool-behavior-migration.md) · [`plans/2026-09-19-scene3d-world-gpuscene.md`](plans/2026-09-19-scene3d-world-gpuscene.md) · [`plans/2026-09-28-scene3d-index-octree.md`](plans/2026-09-28-scene3d-index-octree.md) · [`plans/2026-09-29-legacy-mfc-ex-feature-pack.md`](plans/2026-09-29-legacy-mfc-ex-feature-pack.md) · [`plans/2026-10-02-legacy-ui-inspect-deep-layer.md`](plans/2026-10-02-legacy-ui-inspect-deep-layer.md) · [`plans/2026-09-29-legacy-core-subdirectory-layout.md`](plans/2026-09-29-legacy-core-subdirectory-layout.md) · [`plans/2026-09-29-legacy-plugin-subdirectory-layout.md`](plans/2026-09-29-legacy-plugin-subdirectory-layout.md) |
| RHI + dual scene + map2d + atmosphere + model/compute | [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) | [`plans/2026-09-13-render-rhi-scene.md`](plans/2026-09-13-render-rhi-scene.md) · [`plans/2026-09-27-gpu-rhi-accelerate.md`](plans/2026-09-27-gpu-rhi-accelerate.md) · [`plans/2026-10-02-src-render-vista-parallel-accelerate.md`](plans/2026-10-02-src-render-vista-parallel-accelerate.md) · [`plans/2026-09-27-render-frame-graph.md`](plans/2026-09-27-render-frame-graph.md) · equal-profile + leftover GDI/rhi2d/rhi3d（**Scenic**）checklists on the umbrella **Plans:** line |
| `app/views` shell + Views toolkit + markup + panels + debug console | [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) | [`plans/2026-09-20-m0-views-main-path.md`](plans/2026-09-20-m0-views-main-path.md) · [`plans/2026-09-28-debug-console.md`](plans/2026-09-28-debug-console.md) · compositor / forensics / markup / harness on the umbrella |
| Tool dispatch + `src/tool` | [`specs/2026-09-13-tool-event-dispatch-design.md`](specs/2026-09-13-tool-event-dispatch-design.md) | [`plans/2026-09-13-tool-event-dispatch.md`](plans/2026-09-13-tool-event-dispatch.md) |
| `src/base` foundation (+ memory / PA-E / execution / codecs) | [`specs/2026-09-14-base-root-hybrid-design.md`](specs/2026-09-14-base-root-hybrid-design.md) | [`plans/2026-09-28-partition-alloc-everywhere.md`](plans/2026-09-28-partition-alloc-everywhere.md) · [`plans/2026-09-28-base-execution.md`](plans/2026-09-28-base-execution.md) |
| GIS datasource / GDAL / SDB / Session+Provider | [`specs/2026-09-13-gdal-layer-management-design.md`](specs/2026-09-13-gdal-layer-management-design.md) | [`plans/2026-09-28-datasource-session-provider.md`](plans/2026-09-28-datasource-session-provider.md) · [`plans/2026-09-13-ogr-db-datasource.md`](plans/2026-09-13-ogr-db-datasource.md) · [`plans/2026-09-19-sdbd-wsl-client.md`](plans/2026-09-19-sdbd-wsl-client.md) |
| Plugin host / contributions / store | [`specs/2026-09-13-plugin-host-design.md`](specs/2026-09-13-plugin-host-design.md) | [`plans/2026-09-30-stormsurge-3d-disaster.md`](plans/2026-09-30-stormsurge-3d-disaster.md) · world3d / analysis / leftover-plugin checklists still open on the umbrella |
| Algorithm layer (OSS) | [`specs/2026-09-13-algorithm-layer-oss-design.md`](specs/2026-09-13-algorithm-layer-oss-design.md) | [`plans/2026-09-13-algorithm-layer-oss.md`](plans/2026-09-13-algorithm-layer-oss.md) |
| Net (asio / httplib) | [`specs/2026-09-13-net-asio-httplib-design.md`](specs/2026-09-13-net-asio-httplib-design.md) | [`plans/2026-09-13-net-asio-httplib.md`](plans/2026-09-13-net-asio-httplib.md) |

Open milestone: [`plans/2026-09-20-m1-carto-style-tile-export.md`](plans/2026-09-20-m1-carto-style-tile-export.md). Landed M2–M4 + other completed checklists: [`archive/plans/`](archive/plans/).

## Archive

Landed / superseded / fold-B children: [`archive/`](archive/).

**最后更新:** 2026-10-04（living 仍 9 行；§Scenic 产品 `scenic.dll` + content 宿主；无新 dated spec）
