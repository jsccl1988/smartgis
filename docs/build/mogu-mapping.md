<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# mogu → smartgis 工程管理对照

文档索引：[`../README.md`](../README.md)。Source of truth: WSL `/home/ccl/dev/src/mogu`（本机未检出）。本仓工程入口是 **GN**，不是 sln。

| mogu | smartgis | 状态 |
| --- | --- | --- |
| `build.sh` + `m`/`te`/`a`/`b` | 根目录 `build.bat`（同别名） | 已建 |
| `BUILD.gn` 根组 | 同 | 已建 |
| `.gn` → `//build/BUILDCONFIG.gn` | 同（Windows toolchain 经 mgis 适配） | 已建 |
| 模块树在仓库根（`base/` …） | Foundation 真源在 **`src/base/`**（`//src/base:foundation`）；仓库根 `base/` 仅薄 alias。GIS / UI / render 等产品仍在 **`src/`** 分层。对照 [`src-layout.md`](src-layout.md)、[`../superpowers/specs/2026-09-14-base-root-hybrid-design.md`](../superpowers/specs/2026-09-14-base-root-hybrid-design.md) | Phases 0–6 收口；真源已迁入 `src/` |
| `//base:base`（static / header-mostly） | 首选 **`//src/base:foundation`**（`//base:base` 仍转发）。**不是**产品 DLL。产品平台 DLL **`dll_stem=platform`**（已落地） | Hybrid 收口 + under-`src/` 迁址 |
| `third_party/` | `third_party/`（`manifest.json` + 源码在 `.src/`；薄 `BUILD.gn` 转发 `//third_party:<name>` → `third_party/gn/`；GIS pin 复用 mgis Gitea） | 已建 |
| `third_party/.install` / `build t` 装 prefix | **对齐**；`build.bat t` → `tools/batch.py` 装到 `.install`。Windows 本地 fallback：`.install` 可 junction → mgis `out/third_party`。`out/third_party` 可再 junction 到 `.install`（运行时搜 DLL）；GN 吃 `.install`。不在 ninja 里 cmake sqlite3/PROJ/gdal | 已对齐 |
| `out/` + `out/build.log` | 同 | 已建 |
| Bazel | **不搬** | 跳过 |
| — | `vs2008/` / `branches/` | **已删除**；入口只有 GN |
| — | `build.bat app` / `//:smartgis` | `out/SmartGis.exe`（MFC Feature Pack `CMFC*` via `bcg_cmfc.h`；不在 GN `group("all")`） |
| Chromium `ui/views` + Skia | `src/ui/views` + `src/render/skia`，组 `//:ui_views` | **终局桌面 UI**（本机 mogu Views 树未检出；对齐 Chromium 名 + 本仓 `src/ui` / `src/render`）。对照 [`ui-views-skia.md`](ui-views-skia.md) |
| Qt / WinUI / WebView2 / Feature Pack-as-endgame | — | **不采用**（Feature Pack 只是当前能编通的遗留壳，不是终局） |
| mogu `base::mutex` | — | **不搬**；新树 `std::mutex` |

---

**最后更新：** 2026-09-15
