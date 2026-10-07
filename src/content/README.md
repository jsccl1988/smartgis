<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content` — embedder API + browser session

One product DLL (`dll_stem=content`). Embedders include public headers and `GisContentsClient` only.
Child-process launch lives in `content/browser/child`. Product headers do not include renderer/gpu headers.
In-process browser session (`:browser_session` and present/GDI) is a **source_set**:
Views/exe link it; it is **not** absorbed into `content.dll` (except the
exported Scene3d engine SoT — see `browser/present/scene3d/session/`).

```
content/
  public/          # 7 embedder headers (abstract GisDocument; no gis_bootstrap / GisSceneDocument)
  app/             # ContentMain + process-type switch
  browser/         # C11: contents/session/document/camera/present/capability/debug/child
  renderer/        # --type=renderer child
  view/            # in-process ToolSession + LocalToolRouter
  common/          # host pipe + frame ABI
```

Browser internals: [`browser/README.md`](browser/README.md). Present stack:
[`browser/present/README.md`](browser/present/README.md). Debug agent:
[`browser/debug/README.md`](browser/debug/README.md).

Layout as-built: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md)
(Hosted map row). Living lock: views-desktop-shell **§Content sink** + **§Content browser subdirectory tighten**.

Namespaces stay `content` (internals `content::detail`). No Qt; no third
public namespace.

## GN

| Label | Role |
| --- | --- |
| `//src/content:content` | DLL (`content_sources` + tool_session + gis_bootstrap) |
| `:browser_session` | Owns `session/` + document/camera/present + gestures + `GisContents*` |
| `:gis_scene` / `:gis_camera` / `:gis_present` / `:scene3d_present` / `:gis_hwnd_gestures` | Capability source_sets |
| `:debug_agent` / `:capability` | Opt-in console + IL host |

## Verify

```bat
build.bat debug content
build.bat debug browser_session
build.bat debug content_gis_bootstrap_test
build.bat debug scene3d_presenter_test
build.bat debug gis_scene_test
build.bat debug debug_agent_test
```

---

**最后更新：** 2026-10-07
