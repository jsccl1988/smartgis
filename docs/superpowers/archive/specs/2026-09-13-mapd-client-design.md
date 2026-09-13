<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# mapd HTTP client (request surface)

**Date:** 2026-09-13  
**Status:** superseded  
**Superseded by:** product has no web / mapd / WMS stack (deleted 2026-09-13). Layer I/O is `sdb` / GDAL.

## Goal

Let this repo issue mogu **mapd** HTTP requests on `:8020`: health, style, capabilities, Path Q feature query, and Path R tile URL construction. Contract-compatible with mgis `sg::MapdClient` / `QueryRequest`. Callers inject HTTP so tests never need a live daemon.

## Non-goals

- Do not implement `SdbdClient`, catalog, recordset, or `:8021`.
- Do not treat Path Q GeoJSON as an sdbd `FeatureSet` cursor (`open` / `fetch` / `close`).
- Do not `GDALOpenEx` a remote mapd URL. GDAL stays the local layer backend (`gdal-layer-management`).
- Do not rewrite `SmtMapClient` / `Smt2DWSXView` / `plugin/map_service`.
- Do not delete leftover `src/web` WMS/WFS servers.
- Do not vendor mogu, WinHTTP, or a second HTTP stack. Live IO uses `net::HttpClient`.
- Do not invent mapd FnRPC (`sgdata://mapd/fnrpc` is illegal).
- Qt is banned.

## Architecture

```
caller (unit test / later xview)
        │
        ▼
 web::MapdClient(base_url, HttpTransport)
        │  health / get_style / get_capabilities / query / tile_url
        ▼
 web::HttpTransport
        ├── web::FakeHttpTransport      contract tests
        └── web::NetHttpTransport         wraps net::HttpClient → :8020
```

mapd is a separate HTTP Connection from local GDAL and from sdbd. Health failure is `not_ready`. No fallback to sdbd, shapefile, or homemade WMS.

Default base `http://127.0.0.1:8020` (`SG_MAPD_BASE`). Wrong port `:8021` on a mapd link is a parse error, not a successful sdbd call.

Public C++ is two levels: `web`. Transport, JSON, and fake bind live in `web::detail` or an anonymous namespace in a `.cc`. New functions are `snake_case`. Types are PascalCase.

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `web::HttpTransport` | `send(HttpRequest) → HttpResponse` | none |
| `web::FakeHttpTransport` | path-suffix handlers; counts meta/Q/R | transport |
| `web::NetHttpTransport` | GET/POST via `net::HttpClient` | `//src/net:net` |
| `web::QueryRequest` | Path Q whitelist body; `to_json()` | none |
| `web::MapdClient` | health / style / capabilities / query / `tile_url` | transport |
| `web::parse_mapd_link` | `http(s)://` and `mapd://` → `MapdLink` | none |
| `web::detail::json` | capabilities parse only | none |

`create_default_http_transport()` returns `NetHttpTransport` for live checks. Tests pass `FakeHttpTransport`.

### Request contract (mgis / CONSUME_API)

| Method | HTTP | Path class |
| --- | --- | --- |
| `health()` | `GET /mapd/api/v1/health` | meta |
| `get_style()` | `GET /mapd/style.json` | meta |
| `get_capabilities()` | `GET /mapd/api/v1/capabilities` | meta |
| `query(QueryRequest)` | `POST /mapd/api/v1/features/query` | Q |
| `tile_url(kind, z, x, y)` | URL only, no GET this pass | R (constructed) |

Path Q JSON keys (only these): `layer`, `bbox` `[min_lon,min_lat,max_lon,max_lat]`, `lon`, `lat`, `buffer_m`, `id`, `filters[]` `{column,op,value}`, `limit`. No `sql` / `raw_sql` / `raw_query` fields on `QueryRequest`. Fake/live servers that see those keys return **400** `forbidden_sql`.

`tile_url`:

| `TileKind` | path |
| --- | --- |
| `kRaster` | `/mapd/tiles/raster/{z}/{x}/{y}.webp` |
| `kVector` | `/mapd/tiles/vector/{z}/{x}/{y}.mvt` |
| `kTerrain` | `/mapd/tiles/terrain/{z}/{x}/{y}.png` |
| `kSatellite` | `/mapd/tiles/satellite/{z}/{x}/{y}` |

`parse_mapd_link`:

| input | result |
| --- | --- |
| `http://127.0.0.1:8020` | `base_url` that origin |
| `mapd://127.0.0.1:8020/?layer=basemap` | `http://127.0.0.1:8020`, `layer=basemap` |
| `http://127.0.0.1:8021` | false; `error` mentions `sdbd` |
| missing port | fill **8020** |

`parse_capabilities` reads `crs`, `data_version`, `path_q.{features,analysis,postgis_configured,primary_store}`, `path_r.{raster,vector}`, `query_layers[]`. Empty `primary_store` becomes `"postgis"` if `postgis_configured`, else `"table"`. Success if `crs` is non-empty or `service` is present.

## Error handling

| Condition | `MapdResult` |
| --- | --- |
| null transport | `ok=false`, `error=not_ready` |
| transport `error` non-empty | `not_ready` |
| HTTP 503 | `not_ready` |
| other HTTP non-2xx | `HTTP {status}` |
| health non-2xx | force `not_ready` (even if the generic string was `HTTP …`) |

No exceptions from exported client methods. No fallback Connection.

## Testing

`src/web/mapd/mapd_client_test.cc` via `test("mapd_client_test")`, registered in `//:test_all`. Same `expect()` harness as `sde_gdal_test` (gtest is not wired).

| Case | Pass |
| --- | --- |
| link parse | http + `mapd://` layer; reject `:8021` |
| `QueryRequest::to_json` | whitelist keys; no sql keys; no `min_x` |
| fake ready | health + capabilities (`EPSG:3857`, `primary_store=table`, `query_layers[0]=basemap`) + style + empty FeatureCollection query |
| health 503 | `error == not_ready` |
| tile URL | raster `…/raster/3/1/2.webp` |
| live | skip unless listening; `SG_MAPD_REQUIRE=1` fails if `not_ready`; `SG_MAPD_SKIP` skips |

Golden capabilities JSON matches mgis `mapd_client_fake_bind`.

## File map

| Path | Responsibility |
| --- | --- |
| `src/web/mapd/http_transport.h` | `HttpRequest` / `HttpResponse` / `HttpTransport` |
| `src/web/mapd/fake_http_transport.h/.cc` | fake + ready bind |
| `src/web/mapd/net_http_transport.cc` | `net::HttpClient` adapter |
| `src/web/mapd/query_request.h/.cc` | whitelist JSON |
| `src/web/mapd/mapd_client.h/.cc` | client |
| `src/web/mapd/mapd_link.h/.cc` | link parse |
| `src/web/mapd/json_lite.h/.cc` | capabilities JSON |
| `src/web/mapd/mapd_client_test.cc` | contract tests |
| `src/web/mapd/BUILD.gn` | `source_set` + test |
| `src/web/BUILD.gn`, `src/BUILD.gn`, `BUILD.gn` | group + `src_all` + `test_all` |

## Self-review

1. Placeholders: none. Wiring xview / deleting WMS is an explicit later pass.
2. mapd ≠ sdbd: no `:8021` success path; no recordset API.
3. Scope is one `source_set` + one test exe. No new DLL stem.
4. `query()` vs `tile_url` cannot be confused: POST GeoJSON vs constructed Path R URL.
