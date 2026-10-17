// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/batches/map2d_batches.h"

#include <bit>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/core/log.h"
#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "gis/tile/protocol/mercator_math.h"

namespace content {
namespace detail {
namespace {

uint64_t fnv1a_mix(uint64_t hash, uint64_t value) {
  return (hash ^ value) * 1099511628211ull;
}

uint64_t hash_string(uint64_t hash, const std::string& text) {
  return fnv1a_mix(hash, std::hash<std::string>{}(text));
}

// Fields vista::probe_fields / carto labels actually read, plus plugin
// paint keys (heat / color). Hashing and copying every OGR column on
// china_city dominated the first `batches` span (~1.4s Debug).
bool is_carto_batch_field(const std::string& name) {
  // line_kind carries china_city road/river semantics (kind is often "line").
  // heat/color drive plugin interpolate / coalesce fills (flood, traffic,
  // geochem, orthogrid). Dropping them leaves fill-color at opaque black.
  return name == "kind" || name == "line_kind" || name == "class" ||
         name == "fclass" || name == "anno" || name == "name" ||
         name == "adcode" || name == "type" || name == "text" ||
         name == "heat" || name == "color";
}

// Reject null / MSVC debug-heap poison / low canonical addresses before
// bit_cast endpoint mixes. Keep this cheap — VirtualQuery false-negatives
// wiped china batches (ocean-only showcase). Store mutex is the real fix.
bool points_buffer_looks_live(const GisScene::Vertex* data, size_t n) {
  if (n == 0 || data == nullptr) {
    return false;
  }
  const auto addr = reinterpret_cast<uintptr_t>(data);
  if (addr < 0x10000u) {
    return false;
  }
  const uint32_t low = static_cast<uint32_t>(addr & 0xffffffffu);
  return low != 0xddddddddu && low != 0xcdcdcdcdu && low != 0xccccccccu;
}

// Stable fingerprint for GisScene layers; used to reuse POD + batch builds.
uint64_t scene_layers_fingerprint(
    const std::vector<GisScene::Layer>& layers) {
  uint64_t hash = 14695981039346656037ull;
  hash = fnv1a_mix(hash, layers.size());
  size_t total_features = 0;
  size_t total_points = 0;
  for (const GisScene::Layer& layer : layers) {
    hash = hash_string(hash, layer.id);
    hash = hash_string(hash, layer.name);
    hash = fnv1a_mix(hash, layer.visible ? 1u : 0u);
    hash = fnv1a_mix(hash, layer.features.size());
    for (const GisScene::Feature& feature : layer.features) {
      ++total_features;
      const size_t point_n = feature.points.size();
      const GisScene::Vertex* point_data = feature.points.data();
      total_points += point_n;
      hash = fnv1a_mix(hash, static_cast<uint64_t>(feature.kind));
      hash = fnv1a_mix(hash, point_n);
      // Defend half-link / race: size()>0 with null or freed data() made
      // bit_cast AV (0xDD INVALID_POINTER_READ) during china present.
      if (points_buffer_looks_live(point_data, point_n)) {
        const double x0 = point_data[0].x;
        const double y0 = point_data[0].y;
        hash = fnv1a_mix(hash, std::bit_cast<uint64_t>(x0));
        hash = fnv1a_mix(hash, std::bit_cast<uint64_t>(y0));
        if (point_n > 1) {
          const double xn = point_data[point_n - 1].x;
          const double yn = point_data[point_n - 1].y;
          hash = fnv1a_mix(hash, std::bit_cast<uint64_t>(xn));
          hash = fnv1a_mix(hash, std::bit_cast<uint64_t>(yn));
        }
      } else if (point_n > 0) {
        LOGGING(LOG_ERROR,
                "[scene_layers_fingerprint] corrupt points size=%zu data=%p "
                "layer=%s",
                point_n, static_cast<const void*>(point_data),
                layer.name.c_str());
        hash = fnv1a_mix(hash, 0xC0FFEEULL);
      }
      // Line/polygon carto attrs (kind/class) are stable after China seed;
      // hashing every string on the first batches span dominated wall.
      // Point/text still fingerprint label fields (name/anno) for cache.
      // Polygon heat/color change after append and must invalidate.
      if (feature.kind != GisScene::GeomKind::kPoint &&
          feature.kind != GisScene::GeomKind::kText) {
        for (const GisScene::Field& field : feature.fields) {
          if (field.name != "heat" && field.name != "color") {
            continue;
          }
          hash = hash_string(hash, field.name);
          hash = hash_string(hash, field.value);
        }
        continue;
      }
      for (const GisScene::Field& field : feature.fields) {
        if (!is_carto_batch_field(field.name)) {
          continue;
        }
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

vista::BatchGeomKind to_batch_kind(GisScene::GeomKind kind) {
  switch (kind) {
    case GisScene::GeomKind::kLine:
      return vista::BatchGeomKind::kLine;
    case GisScene::GeomKind::kPolygon:
      return vista::BatchGeomKind::kPolygon;
    case GisScene::GeomKind::kText:
      return vista::BatchGeomKind::kText;
    case GisScene::GeomKind::kPoint:
      return vista::BatchGeomKind::kPoint;
  }
  return vista::BatchGeomKind::kPoint;
}

std::vector<vista::BatchLayer> layers_to_pod(
    const std::vector<GisScene::Layer>& layers) {
  std::vector<vista::BatchLayer> pod;
  pod.reserve(layers.size());
  for (const GisScene::Layer& layer : layers) {
    vista::BatchLayer out;
    out.id = layer.id;
    out.name = layer.name;
    out.visible = layer.visible;
    out.features.reserve(layer.features.size());
    for (const GisScene::Feature& feature : layer.features) {
      vista::BatchFeature feat;
      feat.kind = to_batch_kind(feature.kind);
      feat.points.reserve(feature.points.size());
      for (const GisScene::Vertex& p : feature.points) {
        // Stored map Y is -lat. Layout batches are +lat.
        feat.points.push_back(vista::BatchPoint{p.x, -p.y});
      }
      feat.fields.reserve(8);
      for (const GisScene::Field& field : feature.fields) {
        if (!is_carto_batch_field(field.name)) {
          continue;
        }
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

}  // namespace

vista::LayerBatchSet visible_layer_batches(
    const std::vector<GisScene::Layer>& layers, bool use_carto_slots,
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
  // Collapse land→admin ghost doubles once synth is active (zoom≥8). Far
  // overview skips synth entirely (see emit_kept_probes).
  if (gis::tile::zoom_from_scale(scale) >= 8.0) {
    vista::dedupe_admin_boundary_edges(&built);
  }
  cache.batch = vista::clone_layer_batch_set(built);
  cache.batch_scale_bits = scale_bits;
  cache.batch_use_carto = use_carto_slots;
  cache.batch_valid = true;
  return built;
}

}  // namespace detail
}  // namespace content
