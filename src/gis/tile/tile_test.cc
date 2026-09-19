// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "base/core/core.h"
#include "gis/world/tessellate.h"
#include "gis/tile/mvt_stub.h"
#include "gis/tile/provider_tile_layer.h"
#include "gis/tile/source_registry.h"
#include "gis/tile/style_source.h"
#include "gis/tile/tile_map_layer.h"
#include "gis/tile/tile_provider.h"
#include "gis/tile/wmts.h"
#include "gis/tile/xyz_math.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using gis::tile::format_xyz_url;
  using gis::tile::tile_world_rect;
  using gis::tile::TileCoord;
  using gis::tile::TileImage;
  using gis::tile::TileProvider;
  using gis::tile::tiles_for_viewport;
  using gis::tile::Viewport;

  {
    const std::string url =
        format_xyz_url("http://tile.example/{z}/{x}/{y}.png", 3, 1, 2);
    expect(url == "http://tile.example/3/1/2.png", "format_xyz_url z/x/y");
    const std::string sub =
        format_xyz_url("http://{s}.tile.example/{z}/{x}/{y}.png", 0, 0, 0);
    expect(sub == "http://a.tile.example/0/0/0.png",
           "format_xyz_url subdomain");
  }

  {
    const base::fRect r0 = tile_world_rect(0, 0, 0);
    expect(r0.lb.x < 0 && r0.rt.x > 0 && r0.lb.y < 0 && r0.rt.y > 0,
           "z0 world rect spans origin");
    Viewport vp;
    vp.min_x = -20037508.0;
    vp.min_y = -20037508.0;
    vp.max_x = 20037508.0;
    vp.max_y = 20037508.0;
    vp.z = 1;
    const auto coords = tiles_for_viewport(vp);
    expect(coords.size() == 4, "z1 world = 4 tiles");
  }

  {
    auto provider = std::make_shared<TileProvider>();
    expect(!provider->open_xyz("http://example/{z}/{x}"), "reject missing {y}");
    expect(provider->open_xyz("http://127.0.0.1/{z}/{x}/{y}.png"),
           "open_xyz http");
    expect(provider->open_xyz("https://tiles.example/{z}/{x}/{y}.png"),
           "open_xyz https template");

    const char k_png_stub[] = "PNG-STUB-BYTES";
    int http_calls = 0;
    provider->set_fetch_fn([&](const std::string& url) {
      ++http_calls;
      net::HttpResult res;
      expect(url.find("/2/1/1.png") != std::string::npos ||
                 url.find("/2/") != std::string::npos,
             "mock saw xyz url");
      res.ok = true;
      res.status = 200;
      res.body.assign(k_png_stub, sizeof(k_png_stub) - 1);
      return res;
    });

    TileCoord c{2, 1, 1};
    TileImage one = provider->fetch_tile(c);
    expect(one.bytes == k_png_stub, "mock fetch bytes");
    expect(one.world_rect.rt.x > one.world_rect.lb.x, "tile rect area");
    expect(http_calls == 1, "first fetch hits HTTP");
    expect(provider->cache_size() == 1, "cache stores successful tile");

    TileImage two = provider->fetch_tile(c);
    expect(two.bytes == k_png_stub, "LRU hit returns bytes");
    expect(http_calls == 1, "second same-key fetch skips HTTP");

    Viewport vp;
    const base::fRect wr = tile_world_rect(2, 1, 1);
    vp.min_x = wr.lb.x;
    vp.min_y = wr.lb.y;
    vp.max_x = wr.rt.x;
    vp.max_y = wr.rt.y;
    vp.z = 2;
    auto* layer = new gis::tile::ProviderTileLayer(provider);
    expect(layer->refresh_visible(vp), "refresh_visible");
    expect(layer->GetTileCount() >= 1, "tile count");
    const base::SmtTile* t = layer->GetTile(0);
    expect(t && t->pTileBuf && t->lTileBufSize > 0, "smt tile has image");

    gis::TessMesh mesh;
    expect(gis::tessellate_tile_layer(layer, mesh), "tessellate");
    expect(mesh.has_image, "tessellate has_image");

    gis::MapLayer map_layer = gis::tile::make_map_layer(provider);
    expect(map_layer.layer_type() == gis::LYR_TITLE, "MapLayer kind=tile");
    expect(map_layer.leftover() != nullptr, "leftover ProviderTileLayer");
    expect(map_layer.ogr() == nullptr, "tile MapLayer has no OGR");

    gis::MapLayer xyz =
        gis::tile::make_xyz_map_layer("http://tiles.local/{z}/{x}/{y}.png");
    expect(xyz.layer_type() == gis::LYR_TITLE, "make_xyz_map_layer kind=tile");
    expect(xyz.leftover() != nullptr, "make_xyz_map_layer leftover");
    gis::MapLayer bad = gis::tile::make_xyz_map_layer("http://bad/{z}");
    expect(bad.leftover() == nullptr, "make_xyz_map_layer rejects bad tmpl");

    SMT_SAFE_DELETE(layer);
  }

  // Disk cache: miss → HTTP once; clear memory; hit disk → no second HTTP.
  {
    auto provider = std::make_shared<TileProvider>();
    expect(provider->open_xyz("http://disk.local/{z}/{x}/{y}.png"),
           "disk open_xyz");
    const auto dir =
        (std::filesystem::temp_directory_path() / "smartgis_tile_disk_test")
            .string();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    provider->set_disk_cache_dir(dir);
    provider->set_disk_cache_capacity(32);
    provider->set_cache_capacity(8);

    int http_calls = 0;
    provider->set_fetch_fn([&](const std::string&) {
      ++http_calls;
      net::HttpResult res;
      res.ok = true;
      res.status = 200;
      res.body = "DISK-PNG";
      return res;
    });

    TileCoord c{1, 0, 0};
    expect(provider->fetch_tile(c).bytes == "DISK-PNG", "disk first fetch");
    expect(http_calls == 1, "disk first hits HTTP");
    provider->clear_cache();
    expect(provider->cache_size() == 0, "memory cleared");
    expect(provider->fetch_tile(c).bytes == "DISK-PNG", "disk second fetch");
    expect(http_calls == 1, "disk hit skips HTTP");
    provider->clear_disk_cache();
    std::filesystem::remove_all(dir, ec);
  }

  // WMTS Capabilities fixture (no network).
  {
    const char* k_caps =
        "<?xml version=\"1.0\"?>"
        "<Capabilities>"
        "<Contents><Layer>"
        "<Identifier>osm</Identifier>"
        "<ResourceURL format=\"image/png\" resourceType=\"tile\" "
        "template=\"https://wmts.example/{TileMatrix}/{TileCol}/{TileRow}."
        "png\"/>"
        "</Layer></Contents>"
        "</Capabilities>";
    std::string tmpl;
    std::string err;
    expect(gis::tile::parse_wmts_capabilities(k_caps, &tmpl, &err),
           "parse wmts caps");
    expect(tmpl.find("{z}") != std::string::npos &&
               tmpl.find("{x}") != std::string::npos &&
               tmpl.find("{y}") != std::string::npos,
           "wmts placeholders normalized");
    expect(tmpl.find("wmts.example") != std::string::npos, "wmts host kept");

    gis::MapLayer layer =
        gis::tile::make_wmts_map_layer_from_capabilities(k_caps);
    expect(layer.leftover() != nullptr, "make_wmts from caps");

    std::string norm;
    expect(gis::tile::normalize_wmts_url_template(
               "http://h/{TileMatrix}/{TileCol}/{TileRow}.png", &norm),
           "normalize wmts tmpl");
    expect(norm == "http://h/{z}/{x}/{y}.png", "wmts → xyz");
  }

  // Style sources binding: raster parse + registry; vector → explicit reject.
  {
    using gis::tile::SourceRegistry;
    using gis::tile::StyleSourceDesc;
    using gis::tile::StyleSourceStatus;
    using gis::tile::StyleSourceType;

    const char* k_raster =
        "{\"type\":\"raster\","
        "\"tiles\":[\"http://a.tile/{z}/{x}/{y}.png\"],"
        "\"tileSize\":256}";
    StyleSourceDesc one;
    expect(gis::tile::parse_style_source("basemap", k_raster, &one) ==
               StyleSourceStatus::kOk,
           "parse raster source");
    expect(one.id == "basemap", "raster source id");
    expect(one.type == StyleSourceType::kRaster, "raster type");
    expect(one.tile_size == 256, "tileSize 256");
    expect(one.primary_url_template() == "http://a.tile/{z}/{x}/{y}.png",
           "primary url template");
    expect(gis::tile::is_raster_bindable(one), "raster bindable");

    TileProvider opened;
    expect(gis::tile::open_provider_from_source(one, &opened),
           "open_provider_from_source");
    expect(opened.is_open(), "provider open after source");
    gis::MapLayer from_src = gis::tile::make_xyz_map_layer_from_source(one);
    expect(from_src.leftover() != nullptr, "make_xyz_map_layer_from_source");

    const char* k_style =
        "{\"version\":8,\"name\":\"t\","
        "\"sources\":{"
        "\"osm\":{\"type\":\"raster\","
        "\"tiles\":[\"http://osm/{z}/{x}/{y}.png\"],\"tileSize\":256},"
        "\"roads\":{\"type\":\"vector\","
        "\"tiles\":[\"http://v/{z}/{x}/{y}.pbf\"]}"
        "},"
        "\"layers\":[]}";
    std::vector<StyleSourceDesc> many;
    expect(gis::tile::parse_style_sources(k_style, &many) ==
               StyleSourceStatus::kVectorUnsupported,
           "mixed sources reports vector unsupported");
    expect(many.size() == 1 && many[0].id == "osm",
           "raster still collected from mixed style");

    const char* k_vector =
        "{\"type\":\"vector\",\"tiles\":[\"http://v/{z}/{x}/{y}.pbf\"]}";
    StyleSourceDesc vec;
    expect(gis::tile::parse_style_source("roads", k_vector, &vec) ==
               StyleSourceStatus::kVectorUnsupported,
           "vector source explicit reject");
    expect(vec.type == StyleSourceType::kVector, "vector type recorded");
    expect(!gis::tile::is_raster_bindable(vec), "vector not bindable");

    SourceRegistry reg;
    expect(reg.bind_from_json("osm", k_raster) == StyleSourceStatus::kOk,
           "registry bind raster");
    expect(reg.contains("osm") && reg.get("osm") && reg.get("osm")->is_open(),
           "registry has open provider");
    expect(reg.bind_from_json("roads", k_vector) ==
               StyleSourceStatus::kVectorUnsupported,
           "registry rejects vector");
    expect(!reg.contains("roads"), "vector not inserted");
    expect(reg.size() == 1, "registry size after vector reject");
    expect(reg.bind_raster(one) == StyleSourceStatus::kOk, "bind_raster");
    expect(reg.remove("basemap"), "remove basemap");
    reg.clear();
    expect(reg.size() == 0, "registry clear");

    expect(!gis::tile::mvt::decode_tile(nullptr, 0, nullptr),
           "mvt stub decode fails");
    expect(gis::tile::mvt::decode_status() ==
               gis::tile::mvt::DecodeStatus::kNotImplemented,
           "mvt stub status");
    expect(gis::tile::mvt::reject_vector_source() ==
               StyleSourceStatus::kVectorUnsupported,
           "mvt reject_vector_source");
    expect(gis::tile::mvt::non_goal_message() != nullptr, "mvt non_goal msg");
  }

  // Live CDN HTTPS: SKIP without network.
  std::fprintf(stderr, "SKIP: live HTTPS XYZ CDN (mock + scheme covered)\n");

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "tile_test ok\n");
  return 0;
}
