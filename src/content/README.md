<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content` — embedder API + browser session

One product DLL (`dll_stem=content`). Embedders include **`public/`** only.
In-process map session (`:map_session` and present/GDI) is a **source_set**:
Views/exe link it; it is **not** absorbed into `content.dll` (except the
exported Scene3d engine SoT — see `browser/present/scene3d/session/`).

```
content/
  public/          # 9 embedder headers
  app/             # ContentMain + process-type switch
  browser/         # C10: 12 siblings. C11 tighten: contents/session/document/camera/present/capability/debug
  renderer/        # --type=renderer child
  view/            # in-process ViewHost + LocalToolRouter
  embed/           # HWND-free embed sample
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
| `//src/content:content` | DLL (`content_sources` + view_host + embed + map_bootstrap) |
| `:map_session` | Owns `session/` + document/camera/present/input + `MapContents*` |
| `:map_scene` / `:map_camera` / `:map_present` / `:scene3d_present` / `:map_hwnd_gestures` | Capability source_sets |
| `:debug_agent` / `:capability` | Opt-in console + IL host |

## Verify

```bat
build.bat debug content
build.bat debug map_session
build.bat debug content_map_bootstrap_test
build.bat debug scene3d_presenter_test
build.bat debug map_scene_test
build.bat debug debug_agent_test
```

---

**最后更新：** 2026-10-04
