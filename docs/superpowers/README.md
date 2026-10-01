<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Superpowers (in-flight only)

Living decisions live here. As-built product facts live in [`../build/`](../build/) and module READMEs.

**Default: revise the living row.** Do **not** open a new dated `YYYY-MM-DD-*-design.md` or extra plan unless the Gate in `.cursor/rules/repo/superpowers-docs.mdc` is satisfied (new top-level subsystem **and** a written “why § merge is impossible”). Every new requirement continues as a `§` on an existing umbrella below.

## Active living (only these)

| Topic | Living spec | Primary plans |
| --- | --- | --- |
| Leftover strangler SP0–SP5 | [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) | SP1–SP5 under `plans/2026-09-19-legacy-*` / `scene3d-*` / **scene3d-subdirectory-tighten** (§12b) / **scene3d-primitive-deep-layer** (§12c) / `shell-compile-gate` / **legacy-ui-subdirectory-layout** / **legacy-core-subdirectory-layout** / **legacy-mfc-ex-feature-pack** (§11c) / **legacy-tool-bridge-capability-layout** (§SP1) |
| RHI + dual scene + map2d + atmosphere + model/compute | [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) | [`plans/2026-09-13-render-rhi-scene.md`](plans/2026-09-13-render-rhi-scene.md) (+ frame-graph / gpu / P0 / map2d / atmosphere / sky-fog / render-trace / rhi-suite-bench / **map2d-hillshade-line-casing** / **map2d-gap-pin** / **map3d-gap-pin** / **d3d-leftover-capability** / **gl-leftover-capability** / gdi-leftover-worker / gdi-layout-device-compose / gdi-carto-base-math / gdi-leftover-profile / gdi-paint-dedupe / gdi-core-cc-rename / gdi-internal-rhi-reshape / gdi-core-upgrade-dedupe / **gdi-flatten-player** / **legacy-render-pipeline-arena** / **rhi2d-modern-cpp-base-harden** / **rhi2d-chromium-cc-compose** / **rhi2d-leftover-tile-raster** / **rhi2d-eigen-base-math** / **rhi2d-lpdp-hotpath-tune** / **rhi3d-camera-modern-cpp** / **rhi3d-parallel-frame** / **rhi3d-eigen-base-math** / **src-render-map2d-equal-profile-optimize** / **src-render-scene3d-equal-profile-optimize**) |
| `app/views` shell + Views toolkit + markup + panels + debug console | [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) | [`plans/2026-09-20-m0-views-main-path.md`](plans/2026-09-20-m0-views-main-path.md) (+ declarative-markup / gis-panels / debug-console / **gis-python-spatial-analysis** / compositor / **ui-shell-perf-upgrade** / document-map-scene / csd-theme / **ui-visual-forensics** / **map-browse-forensic-harness** / **plugin-report-browser** / **harness-visual-review**) |
| Tool dispatch + `src/tool` | [`specs/2026-09-13-tool-event-dispatch-design.md`](specs/2026-09-13-tool-event-dispatch-design.md) | [`plans/2026-09-13-tool-event-dispatch.md`](plans/2026-09-13-tool-event-dispatch.md), [`plans/2026-09-28-tool-dll-abi-workspace.md`](plans/2026-09-28-tool-dll-abi-workspace.md) |
| `src/base` foundation (+ memory / PA-E / execution / codecs) | [`specs/2026-09-14-base-root-hybrid-design.md`](specs/2026-09-14-base-root-hybrid-design.md) | [`plans/2026-09-28-partition-alloc-everywhere.md`](plans/2026-09-28-partition-alloc-everywhere.md) (+ base-memory / gis-memory-load / base-execution / third-party-json-xml-protobuf / **base-trace-subdir**) |
| GIS datasource / GDAL / SDB / Session+Provider | [`specs/2026-09-13-gdal-layer-management-design.md`](specs/2026-09-13-gdal-layer-management-design.md) | [`plans/2026-09-28-datasource-session-provider.md`](plans/2026-09-28-datasource-session-provider.md) (+ ogr-db / sdbd-wsl / **gis-coverage-benchmark** / **map2d-gap-pin**) |
| Plugin host / contributions / store | [`specs/2026-09-13-plugin-host-design.md`](specs/2026-09-13-plugin-host-design.md) | [`plans/2026-09-13-plugin-host.md`](plans/2026-09-13-plugin-host.md) (+ plugin-product-package-layout / **gis-python-spatial-analysis** / **traffic-flood-analysis-plugins** / **stormsurge-3d-disaster** / **mine-stratum-earthwork** / **geochem-analysis-plugin** / §product–Python / **legacy-plugin-subdirectory-layout** / **plugin-product-sample-viz** / **world3d-pointcloud-las** / **world3d-true-earth** / **orthogrid3d-hexgrid** / **analysis-session-import-save-viz** / **plugin-report-browser**) |
| Algorithm layer (OSS) | [`specs/2026-09-13-algorithm-layer-oss-design.md`](specs/2026-09-13-algorithm-layer-oss-design.md) | [`plans/2026-09-13-algorithm-layer-oss.md`](plans/2026-09-13-algorithm-layer-oss.md) |
| Net (asio / httplib) | [`specs/2026-09-13-net-asio-httplib-design.md`](specs/2026-09-13-net-asio-httplib-design.md) | [`plans/2026-09-13-net-asio-httplib.md`](plans/2026-09-13-net-asio-httplib.md) |

Product milestone checklists (not new topics): [`plans/2026-09-20-m1-carto-style-tile-export.md`](plans/2026-09-20-m1-carto-style-tile-export.md), [`plans/2026-09-27-m2-processing-toolbox.md`](plans/2026-09-27-m2-processing-toolbox.md), [`plans/2026-09-27-m3-city-3d-stream.md`](plans/2026-09-27-m3-city-3d-stream.md), [`plans/2026-09-27-m4-enterprise-edit-embed.md`](plans/2026-09-27-m4-enterprise-edit-embed.md).

## Archive

Landed / superseded / fold-B children: [`archive/`](archive/).

**最后更新:** 2026-10-01（§src_render Scene3d equal-profile；§rhi2d LP↔DP hot-path tune；§rhi3d Eigen frustum/math；§rhi2d Eigen / base/math deepen；§12c scene3d primitive deep layer；§rhi3d leftover parallel frame；§rhi2d leftover tile-raster；§Visual review closed-loop；§IL interaction recorder；§report browser / WebView2 Report dock；§world3d True Earth；§geochem；§stormsurge；§mine；§VS Code UI Markup；§Map2D/3D gap pin；§Map browse forensic；§traffic+flood；§Shell perf）
