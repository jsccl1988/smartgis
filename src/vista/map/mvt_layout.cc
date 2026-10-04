// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/mvt_layout.h"

#include <string>

#include "gis/style/document/style_document.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace {

std::unique_ptr<OGRGeometry> rings_to_ogr(
    gis::tile::MvtGeomType type,
    const std::vector<std::vector<std::pair<double, double>>>& rings,
    double min_x, double min_y, double max_x, double max_y, uint32_t extent) {
  if (rings.empty() || extent == 0) {
    return nullptr;
  }
  const double sx = (max_x - min_x) / static_cast<double>(extent);
  const double sy = (max_y - min_y) / static_cast<double>(extent);
  auto map_xy = [&](double tx, double ty, double* x, double* y) {
    *x = min_x + tx * sx;
    // MVT y grows down; GIS view CRS y grows up.
    *y = max_y - ty * sy;
  };

  if (type == gis::tile::MvtGeomType::kPoint) {
    double x = 0;
    double y = 0;
    map_xy(rings[0][0].first, rings[0][0].second, &x, &y);
    return std::unique_ptr<OGRGeometry>(new OGRPoint(x, y));
  }
  if (type == gis::tile::MvtGeomType::kLineString) {
    auto* line = new OGRLineString();
    for (const auto& ring : rings) {
      for (const auto& p : ring) {
        double x = 0;
        double y = 0;
        map_xy(p.first, p.second, &x, &y);
        line->addPoint(x, y);
      }
    }
    return std::unique_ptr<OGRGeometry>(line);
  }
  if (type == gis::tile::MvtGeomType::kPolygon) {
    auto* poly = new OGRPolygon();
    for (const auto& ring : rings) {
      if (ring.size() < 3) {
        continue;
      }
      OGRLinearRing lr;
      for (const auto& p : ring) {
        double x = 0;
        double y = 0;
        map_xy(p.first, p.second, &x, &y);
        lr.addPoint(x, y);
      }
      lr.closeRings();
      poly->addRing(&lr);
    }
    if (poly->IsEmpty()) {
      delete poly;
      return nullptr;
    }
    return std::unique_ptr<OGRGeometry>(poly);
  }
  return nullptr;
}

std::string default_mvt_style_json(const gis::tile::MvtTile& tile) {
  std::string layers;
  for (const gis::tile::MvtLayer& layer : tile.layers) {
    if (layer.features.empty()) {
      continue;
    }
    const gis::tile::MvtGeomType t = layer.features.front().type;
    if (!layers.empty()) {
      layers += ',';
    }
    if (t == gis::tile::MvtGeomType::kPolygon) {
      layers += "{\"id\":\"" + layer.name +
                "-fill\",\"type\":\"fill\",\"source-layer\":\"" + layer.name +
                "\",\"paint\":{\"fill-color\":\"#88aa66\",\"fill-opacity\":0.7}}";
    } else if (t == gis::tile::MvtGeomType::kPoint) {
      layers += "{\"id\":\"" + layer.name +
                "-circle\",\"type\":\"circle\",\"source-layer\":\"" +
                layer.name +
                "\",\"paint\":{\"circle-radius\":4,\"circle-color\":\"#cc3344\"}}";
    } else {
      layers += "{\"id\":\"" + layer.name +
                "-line\",\"type\":\"line\",\"source-layer\":\"" + layer.name +
                "\",\"paint\":{\"line-color\":\"#2266cc\",\"line-width\":2}}";
    }
  }
  return "{\"version\":8,\"layers\":[" + layers + "]}";
}

}  // namespace

bool mvt_to_layer_batches(const gis::tile::MvtTile& tile, double min_x,
                          double min_y, double max_x, double max_y,
                          std::vector<std::unique_ptr<OGRGeometry>>* holder,
                          std::vector<LayerBatch>* batches) {
  if (!holder || !batches) {
    return false;
  }
  holder->clear();
  batches->clear();
  for (const gis::tile::MvtLayer& layer : tile.layers) {
    LayerBatch batch;
    batch.source_layer = layer.name;
    for (const gis::tile::MvtFeature& feature : layer.features) {
      auto geom = rings_to_ogr(feature.type, feature.rings, min_x, min_y, max_x,
                               max_y, layer.extent);
      if (!geom) {
        continue;
      }
      batch.geoms.push_back(geom.get());
      batch.attrs.push_back(feature.attrs);
      holder->push_back(std::move(geom));
    }
    if (!batch.geoms.empty()) {
      batches->push_back(std::move(batch));
    }
  }
  return !batches->empty();
}

bool decode_mvt_to_map_frame(const uint8_t* data, size_t len, const View& view,
                             double zoom, const char* style_json,
                             MapFrame* out_frame,
                             gis::tile::MvtDecodeStatus* out_status) {
  if (!out_frame) {
    return false;
  }
  gis::tile::MvtTile tile;
  const gis::tile::MvtDecodeStatus st =
      gis::tile::decode_mvt(data, len, &tile);
  if (out_status) {
    *out_status = st;
  }
  if (st != gis::tile::MvtDecodeStatus::kOk) {
    return false;
  }
  std::vector<std::unique_ptr<OGRGeometry>> holder;
  std::vector<LayerBatch> batches;
  if (!mvt_to_layer_batches(tile, view.min_x, view.min_y, view.max_x, view.max_y,
                            &holder, &batches)) {
    if (out_status) {
      *out_status = gis::tile::MvtDecodeStatus::kEmpty;
    }
    return false;
  }
  const std::string style_owned =
      (style_json && style_json[0]) ? std::string(style_json)
                                    : default_mvt_style_json(tile);
  gis::style::StyleDocument doc;
  if (!gis::style::parse_style_document(style_owned, &doc)) {
    return false;
  }
  LayoutInput in;
  in.view = view;
  in.style = &doc;
  in.zoom = zoom;
  const Layout layout;
  *out_frame = layout.build(in, batches);
  return !out_frame->items.empty();
}

}  // namespace vista
