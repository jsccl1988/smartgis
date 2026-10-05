// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_batches.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "ogrsf_frmts.h"

namespace content {
namespace detail {
namespace {

uint64_t fnv1a_mix(uint64_t hash, uint64_t value) {
  return (hash ^ value) * 1099511628211ull;
}

uint64_t hash_string(uint64_t hash, const std::string& text) {
  return fnv1a_mix(hash, std::hash<std::string>{}(text));
}

// Stable fingerprint for MapScene layers; used to reuse POD + batch builds.
uint64_t scene_layers_fingerprint(
    const std::vector<MapScene::Layer>& layers) {
  uint64_t hash = 14695981039346656037ull;
  hash = fnv1a_mix(hash, layers.size());
  size_t total_features = 0;
  size_t total_points = 0;
  for (const MapScene::Layer& layer : layers) {
    hash = hash_string(hash, layer.id);
    hash = hash_string(hash, layer.name);
    hash = fnv1a_mix(hash, layer.visible ? 1u : 0u);
    hash = fnv1a_mix(hash, layer.features.size());
    for (const MapScene::Feature& feature : layer.features) {
      ++total_features;
      total_points += feature.points.size();
      hash = fnv1a_mix(hash, static_cast<uint64_t>(feature.kind));
      hash = fnv1a_mix(hash, feature.points.size());
      hash = fnv1a_mix(hash, feature.fields.size());
      if (!feature.points.empty()) {
        hash = fnv1a_mix(
            hash, std::bit_cast<uint64_t>(feature.points.front().x));
        hash = fnv1a_mix(
            hash, std::bit_cast<uint64_t>(feature.points.front().y));
        if (feature.points.size() > 1) {
          hash = fnv1a_mix(
              hash, std::bit_cast<uint64_t>(feature.points.back().x));
          hash = fnv1a_mix(
              hash, std::bit_cast<uint64_t>(feature.points.back().y));
        }
      }
      for (const MapScene::Field& field : feature.fields) {
        hash = hash_string(hash, field.name);
        hash = hash_string(hash, field.value);
      }
    }
  }
  hash = fnv1a_mix(hash, total_features);
  hash = fnv1a_mix(hash, total_points);
  return hash;
}

uint64_t scale_key_bits(double scale) {
  if (scale == 0.0) {
    return 0;
  }
  return std::bit_cast<uint64_t>(scale);
}

bool bytes_are_ascii(const char* bytes) {
  if (!bytes) {
    return true;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes);
       *p; ++p) {
    if (*p >= 0x80) {
      return false;
    }
  }
  return true;
}

bool field_bytes_are_utf8(const char* bytes) {
  if (!bytes || !bytes[0]) {
    return true;
  }
  if (bytes_are_ascii(bytes)) {
    return true;
  }
  const int wide_len =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, -1, nullptr, 0);
  return wide_len > 1;
}

std::string text_field_utf8(const std::string& bytes) {
  if (field_bytes_are_utf8(bytes.c_str())) {
    return bytes;
  }
  return gis::datasource::ogr_bytes_to_utf8(bytes);
}

struct LayerBatchBuildCache {
  uint64_t scene_fp = 0;
  std::vector<vista::BatchLayer> pod;
  uint64_t batch_scale_bits = 0;
  bool batch_use_carto = false;
  vista::LayerBatchSet batch;
  bool batch_valid = false;
};

LayerBatchBuildCache& batch_build_cache() {
  thread_local LayerBatchBuildCache cache;
  return cache;
}

vista::BatchGeomKind to_batch_kind(MapScene::GeomKind kind) {
  switch (kind) {
    case MapScene::GeomKind::kLine:
      return vista::BatchGeomKind::kLine;
    case MapScene::GeomKind::kPolygon:
      return vista::BatchGeomKind::kPolygon;
    case MapScene::GeomKind::kText:
      return vista::BatchGeomKind::kText;
    case MapScene::GeomKind::kPoint:
      return vista::BatchGeomKind::kPoint;
  }
  return vista::BatchGeomKind::kPoint;
}

std::vector<vista::BatchLayer> layers_to_pod(
    const std::vector<MapScene::Layer>& layers) {
  std::vector<vista::BatchLayer> pod;
  pod.reserve(layers.size());
  for (const MapScene::Layer& layer : layers) {
    vista::BatchLayer out;
    out.id = layer.id;
    out.name = layer.name;
    out.visible = layer.visible;
    out.features.reserve(layer.features.size());
    for (const MapScene::Feature& feature : layer.features) {
      vista::BatchFeature feat;
      feat.kind = to_batch_kind(feature.kind);
      feat.points.reserve(feature.points.size());
      for (const MapScene::Vertex& p : feature.points) {
        // Stored map Y is -lat. Layout batches are +lat.
        feat.points.push_back(vista::BatchPoint{p.x, -p.y});
      }
      feat.fields.reserve(feature.fields.size());
      for (const MapScene::Field& field : feature.fields) {
        vista::BatchField bf;
        bf.name = field.name;
        if (field.name == "name" || field.name == "anno" ||
            field.name == "text") {
          bf.value = text_field_utf8(field.value);
        } else {
          bf.value = field.value;
        }
        feat.fields.push_back(std::move(bf));
      }
      out.features.push_back(std::move(feat));
    }
    pod.push_back(std::move(out));
  }
  return pod;
}

// Adjacent land→admin ring synth strokes the same provincial border twice
// with independent RDP — parallel ghost lines on china overview. Collapse
// admin LineStrings onto unique undirected lon/lat edges (content-side so
// carto present stays green while vista.dll link churn settles).
uint64_t quantize_lonlat(double x, double y) {
  const auto qx = static_cast<int32_t>(std::llround(x * 10000.0));
  const auto qy = static_cast<int32_t>(std::llround(y * 10000.0));
  return (static_cast<uint64_t>(static_cast<uint32_t>(qx)) << 32) |
         static_cast<uint32_t>(qy);
}

struct UndirectedEdgeKey {
  uint64_t a = 0;
  uint64_t b = 0;
  bool operator==(const UndirectedEdgeKey& o) const {
    return a == o.a && b == o.b;
  }
};

struct UndirectedEdgeHash {
  size_t operator()(const UndirectedEdgeKey& e) const {
    return static_cast<size_t>(e.a ^ (e.b * 0x9e3779b97f4a7c15ull));
  }
};

UndirectedEdgeKey make_edge_key(double x0, double y0, double x1, double y1) {
  uint64_t a = quantize_lonlat(x0, y0);
  uint64_t b = quantize_lonlat(x1, y1);
  if (a > b) {
    std::swap(a, b);
  }
  return UndirectedEdgeKey{a, b};
}

void append_unique_line_edges(
    const OGRLineString* line,
    std::unordered_set<UndirectedEdgeKey, UndirectedEdgeHash>* seen,
    vista::LayerBatch* admin, vista::LayerBatchSet* out,
    const std::map<std::string, std::string>& attrs) {
  if (!line || !seen || !admin || !out) {
    return;
  }
  const int n = line->getNumPoints();
  if (n < 2) {
    return;
  }
  auto flush_run = [&](std::unique_ptr<OGRLineString>& run) {
    if (!run || run->getNumPoints() < 2) {
      run.reset();
      return;
    }
    admin->geoms.push_back(run.get());
    out->owned.push_back(std::move(run));
    admin->attrs.push_back(attrs);
  };
  std::unique_ptr<OGRLineString> run;
  for (int i = 1; i < n; ++i) {
    const double x0 = line->getX(i - 1);
    const double y0 = line->getY(i - 1);
    const double x1 = line->getX(i);
    const double y1 = line->getY(i);
    const UndirectedEdgeKey key = make_edge_key(x0, y0, x1, y1);
    if (key.a == key.b || !seen->insert(key).second) {
      flush_run(run);
      continue;
    }
    if (!run) {
      run = std::make_unique<OGRLineString>();
      run->addPoint(x0, y0);
    }
    run->addPoint(x1, y1);
  }
  flush_run(run);
}

void dedupe_admin_boundary_edges(vista::LayerBatchSet* set) {
  if (!set) {
    return;
  }
  vista::LayerBatch* admin = nullptr;
  for (vista::LayerBatch& batch : set->batches) {
    if (batch.source_layer == "admin") {
      admin = &batch;
      break;
    }
  }
  if (!admin || admin->geoms.size() < 2) {
    return;
  }
  std::vector<const OGRGeometry*> old_geoms = std::move(admin->geoms);
  std::vector<std::map<std::string, std::string>> old_attrs =
      std::move(admin->attrs);
  admin->geoms.clear();
  admin->attrs.clear();
  admin->geoms.reserve(old_geoms.size());
  admin->attrs.reserve(old_attrs.size());
  std::unordered_set<UndirectedEdgeKey, UndirectedEdgeHash> seen;
  seen.reserve(old_geoms.size() * 64);
  for (size_t i = 0; i < old_geoms.size(); ++i) {
    const OGRGeometry* raw = old_geoms[i];
    if (!raw) {
      continue;
    }
    const std::map<std::string, std::string>& attrs =
        i < old_attrs.size() ? old_attrs[i]
                             : std::map<std::string, std::string>{};
    const OGRwkbGeometryType type = wkbFlatten(raw->getGeometryType());
    if (type == wkbLineString) {
      append_unique_line_edges(static_cast<const OGRLineString*>(raw), &seen,
                               admin, set, attrs);
    } else if (type == wkbMultiLineString) {
      const auto* multi = static_cast<const OGRMultiLineString*>(raw);
      const int n = multi->getNumGeometries();
      for (int g = 0; g < n; ++g) {
        append_unique_line_edges(
            static_cast<const OGRLineString*>(multi->getGeometryRef(g)), &seen,
            admin, set, attrs);
      }
    } else {
      // Keep non-line admin geoms as-is (unexpected but safe).
      admin->geoms.push_back(raw);
      admin->attrs.push_back(attrs);
    }
  }
}

}  // namespace

vista::LayerBatchSet visible_layer_batches(
    const std::vector<MapScene::Layer>& layers, bool use_carto_slots,
    double scale) {
  LayerBatchBuildCache& cache = batch_build_cache();
  const uint64_t scene_fp = scene_layers_fingerprint(layers);
  const uint64_t scale_bits = scale_key_bits(scale);

  if (cache.scene_fp == scene_fp && cache.batch_valid &&
      cache.batch_scale_bits == scale_bits &&
      cache.batch_use_carto == use_carto_slots) {
    return vista::clone_layer_batch_set(cache.batch);
  }

  if (cache.scene_fp != scene_fp) {
    cache.pod = layers_to_pod(layers);
    cache.scene_fp = scene_fp;
    cache.batch_valid = false;
  }

  vista::LayerBatchSet built =
      vista::build_layer_batches(cache.pod, use_carto_slots, scale);
  dedupe_admin_boundary_edges(&built);
  cache.batch = vista::clone_layer_batch_set(built);
  cache.batch_scale_bits = scale_bits;
  cache.batch_use_carto = use_carto_slots;
  cache.batch_valid = true;
  return built;
}

}  // namespace detail
}  // namespace content
