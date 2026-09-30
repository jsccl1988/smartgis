// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_batches.h"

#include "content/browser/present/map2d/frame/map2d_carto.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/execution/pipeline/pipeline.h"
#include "gis/datasource/provider/impl/ogr/text/ogr_text_encoding.h"

namespace content {
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

const char* feature_field(const MapScene::Feature& f, const char* key) {
  if (!key) {
    return nullptr;
  }
  for (const MapScene::Field& field : f.fields) {
    if (field.name == key) {
      return field.value.c_str();
    }
  }
  return nullptr;
}

std::string feature_display_name(const MapScene::Feature& f) {
  if (const char* anno = feature_field(f, "anno")) {
    if (anno[0]) {
      return anno;
    }
  }
  if (const char* name = feature_field(f, "name")) {
    if (name[0]) {
      return name;
    }
  }
  return {};
}

int feature_label_importance(const MapScene::Feature& f) {
  const char* cls = feature_field(f, "class");
  if (cls) {
    if (std::strcmp(cls, "title") == 0) {
      return 3;
    }
    if (std::strcmp(cls, "region_label") == 0) {
      return 2;
    }
    if (std::strcmp(cls, "river_label") == 0) {
      return 1;
    }
  }
  const std::string name = feature_display_name(f);
  const int by_name = map_scene_place_name_importance(name.c_str());
  if (by_name >= 3) {
    return 3;
  }
  if (const char* adcode = feature_field(f, "adcode")) {
    const size_t n = std::strlen(adcode);
    if (n >= 6) {
      const bool z45 = adcode[4] == '0' && adcode[5] == '0';
      const bool z23 = adcode[2] == '0' && adcode[3] == '0';
      if (z45 && z23) {
        return 3;
      }
      if (z45) {
        return 2;
      }
      if (by_name > 0) {
        return by_name;
      }
      return 1;
    }
  }
  if (by_name > 0) {
    return by_name;
  }
  if (const char* kind = feature_field(f, "kind")) {
    // City / seat points must clear the country-frame label gate (min 2).
    if (std::strcmp(kind, "city") == 0 || std::strcmp(kind, "point") == 0) {
      return 2;
    }
  }
  return 0;
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
    if (has("river") || has("stream") || has("canal") || has("lake")) {
      return "river";
    }
    if (has("admin") || has("bound") || has("border")) {
      return "admin";
    }
    if (has("road") || has("highway") || has("motorway") || has("trunk") ||
        has("primary") || has("secondary") || has("street")) {
      return "road";
    }
    // china_city mixes Natural Earth rivers with sparse roads; unknown lines
    // default to river so country frame is not filled with road casing gold.
    return "river";
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

double polyline_length(const MapScene::Feature& f) {
  if (f.points.size() < 2) {
    return 0.0;
  }
  double len = 0.0;
  for (size_t i = 1; i < f.points.size(); ++i) {
    const double dx = f.points[i].x - f.points[i - 1].x;
    const double dy = f.points[i].y - f.points[i - 1].y;
    len += std::sqrt(dx * dx + dy * dy);
  }
  return len;
}

bool should_keep_feature(const MapScene::Feature& f, const std::string& source,
                         double scale, bool extent_lonlat) {
  if (scale <= 0.0) {
    return true;
  }
  if (source == "label" || f.kind == MapScene::GeomKind::kText ||
      f.kind == MapScene::GeomKind::kPoint) {
    return feature_label_importance(f) >= map_scene_label_min_importance(scale);
  }
  if (f.kind == MapScene::GeomKind::kLine) {
    const char* kind = feature_field(f, "kind");
    const char* cls = feature_field(f, "class");
    if (!cls) {
      cls = feature_field(f, "fclass");
    }
    const MapLineRole role = map_scene_line_role(kind, cls);
    const double len_deg =
        map_scene_length_as_degrees(polyline_length(f), extent_lonlat);
    const bool major = map_scene_line_is_major_class(kind, cls);
    return map_scene_line_visible_at_scale(role, len_deg, major, scale);
  }
  return true;
}

constexpr bool kExtentLonlat = true;
// Pipeline thread spawn cost; below this use parallel_for / serial.
constexpr size_t kPipelineMinLayers = 4;
constexpr size_t kParallelForMinLayers = 2;

void append_layer_features(const MapScene::Layer& layer, bool use_carto_slots,
                           double scale, Map2dBatches* out) {
  if (!out || !layer.visible) {
    return;
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
    if (!should_keep_feature(f, source, scale, kExtentLonlat)) {
      continue;
    }
    gis::vista::LayerBatch* batch = batch_for(out, source);
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
    out->owned.push_back(std::move(geom));
    std::map<std::string, std::string> attrs;
    for (const MapScene::Field& field : f.fields) {
      // Layout next_codepoint / glyph metrics require UTF-8; OGR may hand GBK.
      if (field.name == "name" || field.name == "anno" || field.name == "text") {
        attrs.emplace(field.name,
                      gis::datasource::ogr_bytes_to_utf8(field.value));
      } else {
        attrs.emplace(field.name, field.value);
      }
    }
    // Ensure Layout text-field fallbacks see display name.
    if (attrs.find("anno") == attrs.end() && attrs.find("name") == attrs.end()) {
      std::string display = feature_display_name(f);
      if (!display.empty()) {
        attrs.emplace("name", gis::datasource::ogr_bytes_to_utf8(display));
      }
    }
    batch->attrs.push_back(std::move(attrs));
  }
}

void merge_batches(Map2dBatches* dest, Map2dBatches* src) {
  if (!dest || !src) {
    return;
  }
  for (gis::vista::LayerBatch& batch : src->batches) {
    gis::vista::LayerBatch* out_batch = batch_for(dest, batch.source_layer);
    out_batch->geoms.insert(out_batch->geoms.end(), batch.geoms.begin(),
                            batch.geoms.end());
    out_batch->attrs.insert(out_batch->attrs.end(),
                            std::make_move_iterator(batch.attrs.begin()),
                            std::make_move_iterator(batch.attrs.end()));
  }
  for (auto& g : src->owned) {
    dest->owned.push_back(std::move(g));
  }
  src->batches.clear();
  src->owned.clear();
}

Map2dBatches merge_parts(std::vector<Map2dBatches>* parts) {
  Map2dBatches out;
  if (!parts) {
    return out;
  }
  for (Map2dBatches& part : *parts) {
    merge_batches(&out, &part);
  }
  return out;
}

Map2dBatches batches_via_parallel_for(
    const std::vector<MapScene::Layer>& layers,
    const std::vector<size_t>& visible, bool use_carto_slots, double scale) {
  std::vector<Map2dBatches> parts(visible.size());
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, visible.size(), [&](size_t vi) {
        append_layer_features(layers[visible[vi]], use_carto_slots, scale,
                              &parts[vi]);
      });
  return merge_parts(&parts);
}

// Produce → process layer → sink merge slots (layer order preserved by index).
Map2dBatches batches_via_pipeline(const std::vector<MapScene::Layer>& layers,
                                  const std::vector<size_t>& visible,
                                  bool use_carto_slots, double scale) {
  struct LayerCtx {
    size_t index = 0;
    Map2dBatches part;
  };

  std::vector<Map2dBatches> parts(visible.size());
  std::atomic<size_t> next{0};
  const size_t workers = (std::max)(
      size_t{1},
      (std::min)(visible.size(),
                 static_cast<size_t>((std::max)(1u, std::thread::hardware_concurrency()))));

  using Status = base::execution::detail::Status;
  base::execution::Pipeline<LayerCtx> pipe({
      {1,
       [&](LayerCtx& ctx) -> Status {
         const size_t i = next.fetch_add(1, std::memory_order_relaxed);
         if (i >= visible.size()) {
           return Status::COMPLETE;
         }
         ctx.index = i;
         ctx.part = Map2dBatches{};
         return Status::SUCCESS;
       }},
      {workers,
       [&](LayerCtx& ctx) -> Status {
         append_layer_features(layers[visible[ctx.index]], use_carto_slots,
                               scale, &ctx.part);
         return Status::SUCCESS;
       }},
      {1,
       [&](LayerCtx& ctx) -> Status {
         parts[ctx.index] = std::move(ctx.part);
         return Status::CONSUMED;
       }},
  });
  pipe.set_stage_names({"map2d.batches.produce", "map2d.batches.process",
                        "map2d.batches.sink"});
  pipe.run();
  pipe.wait();
  return merge_parts(&parts);
}

}  // namespace

Map2dBatches visible_layer_batches(const std::vector<MapScene::Layer>& layers,
                                   bool use_carto_slots, double scale) {
  std::vector<size_t> visible;
  visible.reserve(layers.size());
  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].visible) {
      visible.push_back(i);
    }
  }
  if (visible.empty()) {
    return {};
  }
  if (visible.size() >= kPipelineMinLayers) {
    return batches_via_pipeline(layers, visible, use_carto_slots, scale);
  }
  if (visible.size() >= kParallelForMinLayers) {
    return batches_via_parallel_for(layers, visible, use_carto_slots, scale);
  }
  Map2dBatches out;
  out.batches.reserve(1);
  append_layer_features(layers[visible[0]], use_carto_slots, scale, &out);
  return out;
}

}  // namespace detail
}  // namespace content
