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
| Leftover strangler SP0–SP5 | [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) | SP1–SP5 under `plans/2026-09-19-legacy-*` / `scene3d-*` / `shell-compile-gate` |
| RHI + dual scene + map2d + atmosphere + model/compute | [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) | [`plans/2026-09-13-render-rhi-scene.md`](plans/2026-09-13-render-rhi-scene.md) (+ frame-graph / gpu / P0 / map2d / atmosphere / sky-fog / render-trace / **rhi-suite-bench**) |
| `app/views` shell + Views toolkit + markup + panels + debug console | [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) | [`plans/2026-09-20-m0-views-main-path.md`](plans/2026-09-20-m0-views-main-path.md) (+ declarative-markup / gis-panels / debug-console / **gis-python-spatial-analysis** / compositor / document-map-scene / csd-theme / **ui-visual-forensics**) |
| Tool dispatch + `src/tool` | [`specs/2026-09-13-tool-event-dispatch-design.md`](specs/2026-09-13-tool-event-dispatch-design.md) | [`plans/2026-09-13-tool-event-dispatch.md`](plans/2026-09-13-tool-event-dispatch.md), [`plans/2026-09-28-tool-dll-abi-workspace.md`](plans/2026-09-28-tool-dll-abi-workspace.md) |
| `src/base` foundation (+ memory / PA-E / execution / codecs) | [`specs/2026-09-14-base-root-hybrid-design.md`](specs/2026-09-14-base-root-hybrid-design.md) | [`plans/2026-09-28-partition-alloc-everywhere.md`](plans/2026-09-28-partition-alloc-everywhere.md) (+ base-memory / gis-memory-load / base-execution / third-party-json-xml-protobuf) |
| GIS datasource / GDAL / SDB / Session+Provider | [`specs/2026-09-13-gdal-layer-management-design.md`](specs/2026-09-13-gdal-layer-management-design.md) | [`plans/2026-09-28-datasource-session-provider.md`](plans/2026-09-28-datasource-session-provider.md) (+ ogr-db / sdbd-wsl / **gis-coverage-benchmark**) |
| Plugin host / contributions / store | [`specs/2026-09-13-plugin-host-design.md`](specs/2026-09-13-plugin-host-design.md) | [`plans/2026-09-13-plugin-host.md`](plans/2026-09-13-plugin-host.md) (+ plugin-product-package-layout / **gis-python-spatial-analysis** §smartgis.gis / §product–Python division) |
| Algorithm layer (OSS) | [`specs/2026-09-13-algorithm-layer-oss-design.md`](specs/2026-09-13-algorithm-layer-oss-design.md) | [`plans/2026-09-13-algorithm-layer-oss.md`](plans/2026-09-13-algorithm-layer-oss.md) |
| Net (asio / httplib) | [`specs/2026-09-13-net-asio-httplib-design.md`](specs/2026-09-13-net-asio-httplib-design.md) | [`plans/2026-09-13-net-asio-httplib.md`](plans/2026-09-13-net-asio-httplib.md) |

Product milestone checklists (not new topics): [`plans/2026-09-20-m1-carto-style-tile-export.md`](plans/2026-09-20-m1-carto-style-tile-export.md), [`plans/2026-09-27-m2-processing-toolbox.md`](plans/2026-09-27-m2-processing-toolbox.md), [`plans/2026-09-27-m3-city-3d-stream.md`](plans/2026-09-27-m3-city-3d-stream.md), [`plans/2026-09-27-m4-enterprise-edit-embed.md`](plans/2026-09-27-m4-enterprise-edit-embed.md).

## Archive

Landed / superseded / fold-B children: [`archive/`](archive/).

**最后更新:** 2026-09-28（P0–P3 landed + P4 skeleton；Task 7 debug / tool.activate；samples: analysis / product_orchestrate / industry_pack）
