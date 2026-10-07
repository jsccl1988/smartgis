// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/store/layer_store.h"
#include "content/browser/document/gis_scene.h"
#include "content/public/types.h"
#include "gis/tile/provider/tile_provider.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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
  expect(content::layers_to_catalog_json({}) == "[]", "empty array");

  content::LayerDesc a;
  a.id = "layer.a";
  a.name = "Area";
  a.visible = true;
  a.active = true;

  content::LayerDesc b;
  b.id = "layer/b\"x";
  b.name = "Line\nB";
  b.visible = false;

  const std::string json =
      content::layers_to_catalog_json({a, b});
  expect(json ==
             "[{\"id\":\"layer.a\",\"name\":\"Area\",\"visible\":true},"
             "{\"id\":\"layer/b\\\"x\",\"name\":\"Line\\nB\",\"visible\":false}]",
         "two layers with escape");

  // active must not appear on the CEF-compatible wire.
  expect(json.find("active") == std::string::npos, "no active field");
  // kind / children stay off the flat CEF wire this round.
  expect(json.find("kind") == std::string::npos, "no kind field");
  expect(json.find("children") == std::string::npos, "no children field");

  expect(content::json_escape_string("a\"b\\c") == "a\\\"b\\\\c",
         "json_escape_string");

  // Production fill: LayerStore emits kind + china PLPT group nesting.
  {
    content::detail::LayerStore store;
    content::detail::GisLayer area;
    area.id = "china.area";
    area.name = "area";
    area.visible = true;
    area.kind = content::LayerKind::kVector;
    content::detail::GisFeature poly;
    poly.kind = content::detail::GeomKind::kPolygon;
    poly.points = {{0, 0}, {1, 0}, {1, 1}, {0, 0}};
    area.features.push_back(std::move(poly));

    content::detail::GisLayer line;
    line.id = "china.line";
    line.name = "line";
    line.visible = true;
    line.kind = content::LayerKind::kVector;
    content::detail::GisFeature road;
    road.kind = content::detail::GeomKind::kLine;
    road.points = {{0, 0}, {2, 2}};
    line.features.push_back(std::move(road));

    content::detail::GisLayer point;
    point.id = "china.point";
    point.name = "point";
    point.visible = true;
    point.kind = content::LayerKind::kVector;

    content::detail::GisLayer text;
    text.id = "china.text";
    text.name = "text";
    text.visible = true;
    text.kind = content::LayerKind::kVector;

    content::detail::GisLayer basemap;
    basemap.id = "Basemap";
    basemap.name = "Basemap";
    basemap.visible = true;
    basemap.kind = content::LayerKind::kRaster;

    std::vector<content::detail::GisLayer> loaded;
    loaded.push_back(std::move(area));
    loaded.push_back(std::move(line));
    loaded.push_back(std::move(point));
    loaded.push_back(std::move(text));
    loaded.push_back(std::move(basemap));
    store.replace_layers(std::move(loaded));
    store.set_active_layer_id("china.line");

    const std::vector<content::LayerDesc> descs = store.layer_descs();
    expect(descs.size() == 2, "china group + basemap root");
    if (descs.size() == 2) {
      expect(descs[0].id == "group.china", "china group id");
      expect(descs[0].kind == content::LayerKind::kGroup, "china group kind");
      expect(descs[0].children.size() == 4, "china PLPT children");
      expect(descs[0].expanded, "china group expanded");
      bool found_active = false;
      for (const content::LayerDesc& child : descs[0].children) {
        expect(child.kind == content::LayerKind::kVector, "china child vector");
        if (child.id == "china.line") {
          found_active = child.active;
        }
      }
      expect(found_active, "active stays on china.line leaf");
      expect(descs[1].id == "Basemap", "basemap root id");
      expect(descs[1].kind == content::LayerKind::kRaster, "basemap raster");
    }
  }

  // Flat non-china layers keep kind without inventing a group.
  {
    content::detail::LayerStore store;
    expect(store.create_layer("Roads", content::LayerKind::kVector),
           "create vector");
    expect(store.create_layer("Tiles", content::LayerKind::kRaster),
           "create raster");
    const std::vector<content::LayerDesc> descs = store.layer_descs();
    expect(descs.size() == 2, "flat two roots");
    if (descs.size() == 2) {
      expect(descs[0].kind == content::LayerKind::kVector, "roads vector");
      expect(descs[0].children.empty(), "roads no children");
      expect(descs[1].kind == content::LayerKind::kRaster, "tiles raster");
    }
  }

  // Feature-bearing unknown kind still exports as vector.
  {
    content::detail::LayerStore store;
    content::detail::GisLayer layer;
    layer.id = "path.shp#0";
    layer.name = "parcels";
    layer.visible = true;
    content::detail::GisFeature f;
    f.kind = content::detail::GeomKind::kPolygon;
    f.points = {{0, 0}, {1, 0}, {1, 1}, {0, 0}};
    layer.features.push_back(std::move(f));
    store.replace_layers({std::move(layer)});
    const std::vector<content::LayerDesc> descs = store.layer_descs();
    expect(descs.size() == 1, "one inferred leaf");
    if (!descs.empty()) {
      expect(descs[0].kind == content::LayerKind::kVector, "infer vector");
      expect(descs[0].children.empty(), "inferred flat");
    }
  }

  // GisScene injects a synthetic basemap raster when only TileProvider is set.
  {
    content::GisScene scene;
    expect(scene.create_layer("Roads", "polygon"), "scene create roads");
    auto provider = std::make_shared<gis::tile::TileProvider>();
    // open_xyz may fail offline; injection keys off has_basemap_provider which
    // requires is_open(). Use a template that open_xyz accepts.
    if (provider->open_xyz("http://tiles.local/{z}/{x}/{y}.png")) {
      scene.set_basemap_provider(provider);
      const std::vector<content::LayerDesc> descs = scene.layer_descs();
      expect(descs.size() >= 2, "basemap + roads");
      bool found_synth = false;
      for (const content::LayerDesc& d : descs) {
        if (d.id == "basemap.tiles") {
          found_synth = (d.kind == content::LayerKind::kRaster);
        }
      }
      expect(found_synth, "synthetic basemap raster leaf");
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("content_catalog_layers_test: ok\n");
  return 0;
}
