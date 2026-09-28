// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/map2d/frame/map2d_batches.h"

#include <map>
#include <string>
#include <utility>

namespace app {
namespace detail {
namespace {

bool is_carto_slot(const std::string& name) {
  return name == "land" || name == "water" || name == "river" ||
         name == "admin" || name == "road" || name == "label";
}

std::string ascii_lower(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return text;
}

// Default carto style keys source-layer, not the MapScene layer title.
std::string carto_source_layer(const MapScene::Feature& feature) {
  std::string kind;
  for (const MapScene::Field& field : feature.fields) {
    if (field.name == "kind" || field.name == "type" || field.name == "class" ||
        field.name == "fclass") {
      kind = ascii_lower(field.value);
      break;
    }
  }
  const auto has = [&](const char* token) {
    return kind.find(token) != std::string::npos;
  };
  if (feature.kind == MapScene::GeomKind::kPolygon) {
    if (has("water") || has("lake") || has("sea") || has("ocean")) {
      return "water";
    }
    return "land";
  }
  if (feature.kind == MapScene::GeomKind::kLine) {
    if (has("river") || has("stream") || has("canal")) {
      return "river";
    }
    if (has("admin") || has("bound")) {
      return "admin";
    }
    return "road";
  }
  return "label";
}

gis::vista::LayerBatch* batch_for(Map2dBatches* out, const std::string& source) {
  for (gis::vista::LayerBatch& batch : out->batches) {
    if (batch.source_layer == source) {
      return &batch;
    }
  }
  gis::vista::LayerBatch batch;
  batch.source_layer = source;
  out->batches.push_back(std::move(batch));
  return &out->batches.back();
}

}  // namespace

Map2dBatches visible_layer_batches(const std::vector<MapScene::Layer>& layers,
                                   bool use_carto_slots) {
  Map2dBatches out;
  out.batches.reserve(layers.size());
  for (const MapScene::Layer& layer : layers) {
    if (!layer.visible) {
      continue;
    }
    const std::string layer_slot = layer.name.empty() ? layer.id : layer.name;
    const bool layer_is_slot = use_carto_slots && is_carto_slot(layer_slot);
    for (const MapScene::Feature& f : layer.features) {
      if (f.points.empty()) {
        continue;
      }
      const std::string source =
          !use_carto_slots ? layer_slot
                           : (layer_is_slot ? layer_slot : carto_source_layer(f));
      gis::vista::LayerBatch* batch = batch_for(&out, source);
      std::unique_ptr<OGRGeometry> geom;
      if (f.kind == MapScene::GeomKind::kLine) {
        auto line = std::make_unique<OGRLineString>();
        for (const MapScene::Vertex& p : f.points) {
          line->addPoint(p.x, -p.y);
        }
        geom = std::move(line);
      } else if (f.kind == MapScene::GeomKind::kPolygon) {
        auto ring = std::make_unique<OGRLinearRing>();
        for (const MapScene::Vertex& p : f.points) {
          ring->addPoint(p.x, -p.y);
        }
        ring->closeRings();
        auto poly = std::make_unique<OGRPolygon>();
        poly->addRingDirectly(ring.release());
        geom = std::move(poly);
      } else {
        // Points and kText. Symbol layers turn name attrs into frame kText.
        geom = std::make_unique<OGRPoint>(f.points.front().x, -f.points.front().y);
      }
      batch->geoms.push_back(geom.get());
      out.owned.push_back(std::move(geom));
      std::map<std::string, std::string> attrs;
      for (const MapScene::Field& field : f.fields) {
        attrs.emplace(field.name, field.value);
      }
      batch->attrs.push_back(std::move(attrs));
    }
  }
  return out;
}

}  // namespace detail
}  // namespace app
