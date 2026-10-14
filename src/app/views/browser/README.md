<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `app/views/browser` — shell controller layers

`Browser` owns **PluginShell + BrowserUiDelegate** and forwards map document /
camera / present / gestures to `content::BrowserSession`. Keep the **core path**
thin; product China face and harness gates stay in named side TUs.

Living shell: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md).
Nav TU map: [`nav/README.md`](nav/README.md).
Plugin role dirs: [`plugin/README.md`](plugin/README.md).

## Layer inventory (keep this shape)

| Layer | Role | Own here | Do not grow into |
| --- | --- | --- | --- |
| **Core** | Lifecycle, UI forwards, generic nav/commands, PluginHost plumbing | `browser.{h,cc}`, `ui_delegate.h`, `nav/{draft,extent,view,gesture}_nav.cc`, `commands/*` (except harness), `plugin/{shell,host,present,detail}/…` | China sample policy, showcase env switches |
| **Product China** | Mainland sample face: carto drop, framing, atmosphere/orbit, deferred OGR seed | `china_product_defaults.{h,cc}`, `nav/deferred_china_seed.cc` | Inline branches in every nav/command TU |
| **Harness / showcase** | Self-test / M2 / report fake / frame export | `commands/session_commands.cc` (`run_m2_harness_hooks`), `plugin/harness/report_suite.*`, playback export APIs | Product interactive defaults |

Approx size (non-test `.cc`/`.h`, ~5.5k lines / 49 files): **core ~70%**,
**China product dedicated ~6%** (`china_product_defaults*` + `deferred_china_seed`),
**harness/showcase ~8%**, remainder defensive SEH / layout-skew comments and
bridge glue. China **call sites** also sit in core TUs (below) — prefer
funneling new logic into the Product China column.

## Core path (must stay readable)

```
Browser::init
  → Session.init_tool_sessions
  → PluginShell.init (+ optional GisContents / PluginHost)
  → create_browser_ui → init_shell
Browser::show
  → ui.show_shell
  → fit_map_extent (generic framing; China branch is a single call)
  → note_product_map2d_surface_after_show (+ optional native invalidate)
  → schedule_deferred_china_seed (no-op unless defer flag)
  → refresh_scale / navigation baseline
Browser::prepare_close
  → cancel_deferred_china_seed → UI / session teardown
nav / commands
  → draft · extent · view · gesture · catalog · document · view menus
```

`Browser` is a **facade**: session types stay behind out-of-line accessors so
parallel ninja + stale `.obj` layout skew does not AV UI pointers.

## Product China — dedicated surface

| API / TU | Responsibility |
| --- | --- |
| `ensure_china_maplibre_carto` | Drop `china_city.style.json` area/line/point when no product slots |
| `frame_china_map2d` / `apply_china_map2d_product_defaults` | Mainland framing + carto |
| `apply_china_scene3d_atmosphere` / `_orbit` / `_product_defaults` | DEM + atmo flags + orbit |
| `apply_china_scene3d_legacy_look` | Opt-in leftover stereo look |
| `note_product_map2d_surface_after_show` | Session latch drop after sync China seed |
| `should_sync_orbit_extent` | Orbit ExtentChanged filter (China lon/lat only) |
| `Browser::schedule_deferred_china_seed` | Idle China OGR open after first show |
| `Browser::cancel_deferred_china_seed` | Kill idle seed timer on close |

Env / switches owned by this layer (do not re-scatter): `scene3d-atmo`,
`scene3d-land-only`, `skip-china-map2d-defaults`,
`defer_china_seed` / `skip-ambox-catalog` (seed scheduling).

PluginHost sinks take **neutral callbacks** (`apply_product_defaults` /
`apply_atmosphere` on `plugin/host/host_context.h`) filled in
`plugin/host/plugin_host_wiring.cc` — bridges stay product-agnostic at the call site.

## Intentional hooks still inside core TUs

These are **allowed injection points** (one call / filter each). New policy
goes into `china_product_defaults` or `deferred_china_seed`, not more branches.

| Site | Why it stays |
| --- | --- |
| `extent_nav.cc` `fit_map_extent` | Single `apply_china_map2d_product_defaults` vs generic frame |
| `browser.cc` `show` | Harness skip-fit; one call to `note_product_*` + UI invalidate |
| `browser.cc` `OnExtentChanged` / `apply_extent_changed_on_ui` | `should_sync_orbit_extent` + PostMessage to UI thread |
| `browser.cc` `prepare_close` | One call to `cancel_deferred_china_seed` |
| `document_commands.cc` `on_open` | Post-open China-aware refresh |
| `plugin/host/plugin_host_wiring.cc` | Fill product/atmosphere callbacks once |

## Harness / showcase (keep off interactive defaults)

| Site | Notes |
| --- | --- |
| `run_m2_harness_hooks` | Tiny GeoJSON fixtures; not china_city |
| `plugin/harness/report_suite.*` | Fake report browser for showcase command |
| `refresh_scale` early-out on `map2d-showcase` | Avoid StatusBar/Label init race |
| `export_plugin_frames` / `apply_plugin_frame` | ResultPlayback BMPs |

## Rules for edits

1. **New China face / seed / orbit / carto policy** → `china_product_defaults` or
   `deferred_china_seed` only.
2. **New interactive nav/command behavior** → matching `nav/` or `commands/` TU;
   no China sample hardcoding unless using the injection table above.
3. **New PluginHost capability** → generic callback name on
   `plugin/host/host_context.h` when possible; wire product defaults in
   `plugin/host/plugin_host_wiring.cc`.
4. **Do not** pull showcase env checks into `draft_nav` / `gesture_nav` /
   `view_nav` / catalog unless the race is documented and scoped like
   `refresh_scale`.
5. Product plugins under `src/plugin/product/{map2d,world3d}` may duplicate
   framing for **out-of-process showcase**; interactive shell keeps the
   `china_product_defaults` entry points as the single in-Browser face.
