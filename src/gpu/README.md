<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/gpu`

`--type=gpu` payload. Chrome talks only to `content/public`; this tree owns paint.
Do not `#include` `mln/` or `mbgl/` from `app/` / `content/`.

## Render backends

| Track | How to select | What paints |
| --- | --- | --- |
| **B (default)** | unset `SMT_MAP_BACKEND` | Demo clear + GDI features; 3D stays here / `GpuScene` |
| **A** | `SMT_MAP_BACKEND=a` | 2D basemap via `paint_map_frame` → `paint_track_a_basemap` |

### Track A richness

1. Parse Style JSON with `sdb::style::StyleDocument` (`//src/sdb/style`).
2. Parse Style `sources` with `sdb::tile::parse_style_sources` → `SourceRegistry`
   / `TileProvider` (`//src/sdb/tile`); raster `layer.source` must match a
   source id (missing id → skip that layer).
3. Walk `layers` in order into `PresentTarget`:
   - `background`: `background-color` + `background-opacity`
   - `raster`: fetch via `TileFetchFn` (injected into TileProvider) +
     `raster-opacity` (src-over)
4. **Viewport XYZ mosaic**: when `MapPaintRequest.extent` is valid
   (`xmax > xmin` and `ymax > ymin`), call `sdb::tile::tiles_for_viewport`
   (zoom from `MapPaintRequest.zoom`, or `estimate_zoom` when `zoom < 0`).
   Each visible tile is fetched and blitted by its Web Mercator world rect into
   the present size. Degenerate / empty extent keeps the legacy single tile
   `z/x/y = 0/0/0` stretched to the full surface (old tests stay green).
5. XYZ templates: Style source `tiles[]` wins when `layer.source` is set;
   otherwise `tile_url_templates` (one per unbound raster layer in order), or
   legacy single `tile_url_template`. If the style has no `raster` layers but
   request templates are set, each template is composited as an opaque raster
   pass (keeps `SMT_XYZ_URL` / callers working).
6. No network / no `fetch`: background-only frames still succeed.

`MapPaintRequest::fetch` is the TileProvider hook at the call site — do not add a
second HTTP cache inside gpu. `--type=gpu` (`gpu_main`) injects
`make_net_tile_fetch()` (wraps `net::HttpClient::get`, same stack as
`TileProvider` default) when `SMT_XYZ_URL` / `tile_url_templates` or Style
`sources` are present. Fetch failures return `ok=false`; paint keeps background.

### Hand-test real XYZ over `--type=gpu`

```bat
set SMT_MAP_BACKEND=a
set SMT_XYZ_URL=https://tile.openstreetmap.org/{z}/{x}/{y}.png
REM launch chrome / views host that spawns --type=gpu as usual
```

Expect: Track A paints background, then composites the **viewport XYZ set**
(from the surface extent via `tiles_for_viewport`) over HTTP(S). Before the
host sets a non-degenerate extent, the adapter falls back to a single
`z/x/y = 0/0/0` tile. Unset `SMT_XYZ_URL` → background only, process stays up.
Bad template or offline → same background fallback (no crash).

## MapLibre Native pin (optional)

GN `smt_enable_maplibre=false` by default (not in `src_all` / `all`).

```bat
set SMT_ENABLE_MAPLIBRE=true
build.bat render_backend_test.exe
```

Or `gn gen out --args="is_debug=true smt_enable_maplibre=true"`.

Pin path: `third_party/.src/maplibre-native/include/` (`mbgl/map/map.hpp` or
`mln/map.hpp`). See [`third_party/maplibre/README.md`](../../third_party/maplibre/README.md).

### `mln::Map` still vs adapter

| Path | Role |
| --- | --- |
| **`mln::Map` still** (`maplibre_runtime` → facade) | Background (+ opacity) still-image only; no tile fetch / multi-raster |
| **`maplibre_adapter`** | Authoritative Track A: StyleDocument + ordered rasters + opacities |

When the pin is linked, background-only requests may paint through the facade
(so the link stays live). Requests with templates/`sources` + `fetch` always use
the adapter. Full `mbgl::Map` + HeadlessFrontend / mbgl-core is not built on this
MSVC tree.

## Tests

```bat
build.bat render_backend_test.exe
out\render_backend_test.exe
```

Covers backend env selection, `background-opacity`, multi-raster +
`raster-opacity`, Style `sources` + `layer.source` binding (injected
`TileFetchFn`, no real HTTP / no request templates), viewport XYZ mosaic
(non-`0/0/0` fetch + world-rect blit), degenerate-extent fallback to
`0/0/0`, offline background, and `make_net_tile_fetch` invalid-URL /
miss-keeps-background.
