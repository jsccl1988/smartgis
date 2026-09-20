// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/field_store.h"

#include <algorithm>
#include <cmath>

namespace gis {
namespace atmosphere {
namespace {

bool is_timeless(double t) {
  return std::isnan(t);
}

bool same_time(double a, double b) {
  if (is_timeless(a) && is_timeless(b)) {
    return true;
  }
  if (is_timeless(a) || is_timeless(b)) {
    return false;
  }
  return std::abs(a - b) <= 1e-9;
}

bool mask_valid_at(const FieldLayer& layer, std::size_t idx) {
  if (layer.valid_mask.empty()) {
    return true;
  }
  if (idx >= layer.valid_mask.size()) {
    return false;
  }
  return layer.valid_mask[idx] != 0;
}

std::size_t cell_index(const FieldGrid& grid, int col, int row) {
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
         static_cast<std::size_t>(col);
}

// Spatially sample one layer with lon/lat clamp and bilinear (fallback nearest).
// Returns false when the point has no valid contributing cell.
bool sample_spatial(const FieldLayer& layer, double lon, double lat,
                    float* out) {
  if (!out || layer.grid.empty() ||
      layer.values.size() != layer.grid.cell_count()) {
    return false;
  }
  const FieldGrid& g = layer.grid;
  const double lon_c =
      (std::max)(g.min_lon, (std::min)(g.max_lon, lon));
  const double lat_c =
      (std::max)(g.min_lat, (std::min)(g.max_lat, lat));

  if (g.cols == 1 && g.rows == 1) {
    if (!mask_valid_at(layer, 0)) {
      return false;
    }
    *out = layer.values[0];
    return true;
  }

  double u = 0.0;
  double v = 0.0;
  if (g.cols > 1 && g.max_lon != g.min_lon) {
    u = (lon_c - g.min_lon) / (g.max_lon - g.min_lon) *
        static_cast<double>(g.cols - 1);
  }
  if (g.rows > 1 && g.max_lat != g.min_lat) {
    v = (lat_c - g.min_lat) / (g.max_lat - g.min_lat) *
        static_cast<double>(g.rows - 1);
  }
  u = (std::max)(0.0, (std::min)(static_cast<double>(g.cols - 1), u));
  v = (std::max)(0.0, (std::min)(static_cast<double>(g.rows - 1), v));

  const int i0 = static_cast<int>(std::floor(u));
  const int j0 = static_cast<int>(std::floor(v));
  const int i1 = (std::min)(i0 + 1, g.cols - 1);
  const int j1 = (std::min)(j0 + 1, g.rows - 1);
  const float fu = static_cast<float>(u - static_cast<double>(i0));
  const float fv = static_cast<float>(v - static_cast<double>(j0));

  const std::size_t i00 = cell_index(g, i0, j0);
  const std::size_t i10 = cell_index(g, i1, j0);
  const std::size_t i01 = cell_index(g, i0, j1);
  const std::size_t i11 = cell_index(g, i1, j1);
  const bool v00 = mask_valid_at(layer, i00);
  const bool v10 = mask_valid_at(layer, i10);
  const bool v01 = mask_valid_at(layer, i01);
  const bool v11 = mask_valid_at(layer, i11);

  if (v00 && v10 && v01 && v11) {
    const float a = layer.values[i00] * (1.f - fu) + layer.values[i10] * fu;
    const float b = layer.values[i01] * (1.f - fu) + layer.values[i11] * fu;
    *out = a * (1.f - fv) + b * fv;
    return true;
  }

  // Nearest valid cell when bilinear corners are incomplete.
  const int in = (std::min)(g.cols - 1, (std::max)(0, static_cast<int>(std::lround(u))));
  const int jn = (std::min)(g.rows - 1, (std::max)(0, static_cast<int>(std::lround(v))));
  const std::size_t ni = cell_index(g, in, jn);
  if (mask_valid_at(layer, ni)) {
    *out = layer.values[ni];
    return true;
  }
  return false;
}

struct SourceKey {
  FieldSourceKind kind = FieldSourceKind::kProcedural;
  int priority = 0;
};

bool source_better(const SourceKey& a, const SourceKey& b) {
  if (a.priority != b.priority) {
    return a.priority > b.priority;
  }
  // Tie-break: External over Procedural at equal priority.
  return static_cast<int>(a.kind) < static_cast<int>(b.kind);
}

}  // namespace

FieldStore::FieldStore() = default;
FieldStore::~FieldStore() = default;

void FieldStore::set_layer(const FieldLayer& layer) {
  // Replace the entire (channel, kind, priority) group (all time slices).
  layers_.erase(std::remove_if(layers_.begin(), layers_.end(),
                               [&](const FieldLayer& existing) {
                                 return existing.channel == layer.channel &&
                                        existing.kind == layer.kind &&
                                        existing.priority == layer.priority;
                               }),
                layers_.end());
  layers_.push_back(layer);
}

bool FieldStore::upload_slice(FieldChannel channel, FieldSourceKind kind,
                              int priority, const FieldGrid& grid,
                              const float* values, std::size_t value_count,
                              const uint8_t* valid_mask, std::size_t mask_count,
                              double time_sec) {
  if (!values || grid.empty() || value_count != grid.cell_count()) {
    return false;
  }
  if (valid_mask && mask_count != 0 && mask_count != grid.cell_count()) {
    return false;
  }

  for (FieldLayer& existing : layers_) {
    if (existing.channel == channel && existing.kind == kind &&
        existing.priority == priority && same_time(existing.time_sec, time_sec)) {
      existing.grid = grid;
      existing.values.assign(values, values + value_count);
      if (valid_mask && mask_count == grid.cell_count()) {
        existing.valid_mask.assign(valid_mask, valid_mask + mask_count);
      } else {
        existing.valid_mask.clear();
      }
      existing.time_sec = time_sec;
      return true;
    }
  }

  FieldLayer layer;
  layer.channel = channel;
  layer.kind = kind;
  layer.priority = priority;
  layer.grid = grid;
  layer.values.assign(values, values + value_count);
  if (valid_mask && mask_count == grid.cell_count()) {
    layer.valid_mask.assign(valid_mask, valid_mask + mask_count);
  }
  layer.time_sec = time_sec;
  layers_.push_back(std::move(layer));
  return true;
}

float FieldStore::sample(FieldChannel channel, double lon, double lat,
                         double time_sec) const {
  // Collect distinct (kind, priority) sources for this channel.
  std::vector<SourceKey> keys;
  for (const FieldLayer& layer : layers_) {
    if (layer.channel != channel) {
      continue;
    }
    SourceKey key{layer.kind, layer.priority};
    bool found = false;
    for (const SourceKey& existing : keys) {
      if (existing.kind == key.kind && existing.priority == key.priority) {
        found = true;
        break;
      }
    }
    if (!found) {
      keys.push_back(key);
    }
  }
  std::sort(keys.begin(), keys.end(), source_better);

  for (const SourceKey& key : keys) {
    std::vector<const FieldLayer*> slices;
    const FieldLayer* timeless = nullptr;
    for (const FieldLayer& layer : layers_) {
      if (layer.channel != channel || layer.kind != key.kind ||
          layer.priority != key.priority) {
        continue;
      }
      if (is_timeless(layer.time_sec)) {
        timeless = &layer;
        continue;
      }
      slices.push_back(&layer);
    }

    // Pure timeless source (no timed slices).
    if (slices.empty()) {
      if (!timeless) {
        continue;
      }
      float value = 0.f;
      if (sample_spatial(*timeless, lon, lat, &value)) {
        return value;
      }
      continue;
    }

    std::sort(slices.begin(), slices.end(),
              [](const FieldLayer* a, const FieldLayer* b) {
                return a->time_sec < b->time_sec;
              });

    // Temporal clamp + linear lerp between bracketing slices.
    const FieldLayer* a = slices.front();
    const FieldLayer* b = slices.back();
    float t_blend = 0.f;
    if (time_sec <= slices.front()->time_sec) {
      a = b = slices.front();
      t_blend = 0.f;
    } else if (time_sec >= slices.back()->time_sec) {
      a = b = slices.back();
      t_blend = 0.f;
    } else {
      for (std::size_t i = 0; i + 1 < slices.size(); ++i) {
        if (time_sec >= slices[i]->time_sec &&
            time_sec <= slices[i + 1]->time_sec) {
          a = slices[i];
          b = slices[i + 1];
          const double span = b->time_sec - a->time_sec;
          t_blend = (span > 0.0)
                        ? static_cast<float>((time_sec - a->time_sec) / span)
                        : 0.f;
          break;
        }
      }
    }

    float va = 0.f;
    float vb = 0.f;
    const bool ok_a = sample_spatial(*a, lon, lat, &va);
    const bool ok_b = (a == b) ? ok_a : sample_spatial(*b, lon, lat, &vb);
    if (ok_a && ok_b) {
      if (a == b) {
        return va;
      }
      return va * (1.f - t_blend) + vb * t_blend;
    }
    if (ok_a) {
      return va;
    }
    if (ok_b) {
      return vb;
    }

    // Timed slices invalid at this point: fall back to timeless peer.
    if (timeless) {
      float value = 0.f;
      if (sample_spatial(*timeless, lon, lat, &value)) {
        return value;
      }
    }
  }

  return 0.0f;
}

bool FieldStore::timed_slice_range(FieldChannel channel, double* out_min,
                                   double* out_max) const {
  if (!out_min || !out_max) {
    return false;
  }
  bool found = false;
  double t_min = 0.0;
  double t_max = 0.0;
  for (const FieldLayer& layer : layers_) {
    if (layer.channel != channel || is_timeless(layer.time_sec)) {
      continue;
    }
    if (!found) {
      t_min = t_max = layer.time_sec;
      found = true;
    } else {
      t_min = (std::min)(t_min, layer.time_sec);
      t_max = (std::max)(t_max, layer.time_sec);
    }
  }
  if (!found) {
    return false;
  }
  *out_min = t_min;
  *out_max = t_max;
  return true;
}

const FieldLayer* FieldStore::layer_at(std::size_t index) const {
  if (index >= layers_.size()) {
    return nullptr;
  }
  return &layers_[index];
}

}  // namespace atmosphere
}  // namespace gis
