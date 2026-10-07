// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/mesh.h"

#include <algorithm>
#include <cmath>

#include "base/time/elapsed_timer.h"
#include "vista/terrain/dem/dem_bake_cache.h"

namespace vista {
namespace detail {


void stamp_payload_meta(World* world, uint64_t node_id, int lod_key,
                        TerrainSource source) {
  if (!world) {
    return;
  }
  Node* node = world->find(node_id);
  if (!node || node->kind != NodeKind::kTerrain) {
    return;
  }
  node->terrain.lod_key = lod_key;
  node->terrain.source = source;
}

void apply_elevation_overlay_texture(World* world, uint64_t node_id,
                                     const DemRaster& dem, int edge,
                                     std::vector<uint8_t>* rgba, int tw,
                                     int th) {
  if (!world || !rgba || tw < 2 || th < 2) {
    return;
  }
  std::vector<uint8_t> overlay;
  int ow = 0;
  int oh = 0;
  // Jet surface + dark charcoal isolines (paint.cc). White Origin stamps
  // used to wash ui.scene; dark 1px strokes stay readable after bilinear
  // upscale. ContourSheet overlay TIN uses the same bake for its atlas.
  if (!dem.bake_elevation_overlay_rgba(edge, /*surface=*/true, /*curves=*/true,
                                       &overlay, &ow, &oh) ||
      ow != tw || oh != th || overlay.size() != rgba->size()) {
    return;
  }
  for (size_t i = 0; i + 3 < overlay.size(); i += 4) {
    if (overlay[i + 3] == 0) {
      continue;
    }
    (*rgba)[i + 0] = overlay[i + 0];
    (*rgba)[i + 1] = overlay[i + 1];
    (*rgba)[i + 2] = overlay[i + 2];
    (*rgba)[i + 3] = 255;
  }
  world->set_terrain_texture(node_id, rgba->data(), rgba->size(),
                             static_cast<uint32_t>(tw),
                             static_cast<uint32_t>(th));
}

// Keep every |stride|-th triangle from a TessMesh (discrete TIN LOD).
bool thin_tess_mesh(const TessMesh& in, int stride, TessMesh* out) {
  if (!out || in.positions.empty()) {
    return false;
  }
  out->positions.clear();
  out->indices.clear();
  if (in.indices.size() < 3 || (in.indices.size() % 3) != 0) {
    return false;
  }
  const int s = stride > 1 ? stride : 1;
  const size_t tri_count = in.indices.size() / 3;
  out->positions.reserve(in.positions.size() / static_cast<size_t>(s));
  out->indices.reserve(in.indices.size() / static_cast<size_t>(s));
  for (size_t t = 0; t < tri_count; ++t) {
    if ((t % static_cast<size_t>(s)) != 0) {
      continue;
    }
    const uint32_t i0 = in.indices[t * 3 + 0];
    const uint32_t i1 = in.indices[t * 3 + 1];
    const uint32_t i2 = in.indices[t * 3 + 2];
    const size_t b0 = static_cast<size_t>(i0) * 3u;
    const size_t b1 = static_cast<size_t>(i1) * 3u;
    const size_t b2 = static_cast<size_t>(i2) * 3u;
    if (b0 + 2 >= in.positions.size() || b1 + 2 >= in.positions.size() ||
        b2 + 2 >= in.positions.size()) {
      continue;
    }
    const uint32_t base =
        static_cast<uint32_t>(out->positions.size() / 3);
    out->positions.push_back(in.positions[b0]);
    out->positions.push_back(in.positions[b0 + 1]);
    out->positions.push_back(in.positions[b0 + 2]);
    out->positions.push_back(in.positions[b1]);
    out->positions.push_back(in.positions[b1 + 1]);
    out->positions.push_back(in.positions[b1 + 2]);
    out->positions.push_back(in.positions[b2]);
    out->positions.push_back(in.positions[b2 + 1]);
    out->positions.push_back(in.positions[b2 + 2]);
    out->indices.push_back(base);
    out->indices.push_back(base + 1);
    out->indices.push_back(base + 2);
  }
  return out->indices.size() >= 3;
}

bool thin_tess_mesh_spatial(const TessMesh& in, float camera_distance,
                            float cx, float cy, float cz, TessMesh* out) {
  if (!out || in.positions.size() < 9 || in.indices.size() < 3 ||
      (in.indices.size() % 3) != 0) {
    return false;
  }
  float minx = in.positions[0];
  float miny = in.positions[1];
  float minz = in.positions[2];
  float maxx = minx;
  float maxy = miny;
  float maxz = minz;
  for (size_t i = 0; i + 2 < in.positions.size(); i += 3) {
    minx = (std::min)(minx, in.positions[i]);
    miny = (std::min)(miny, in.positions[i + 1]);
    minz = (std::min)(minz, in.positions[i + 2]);
    maxx = (std::max)(maxx, in.positions[i]);
    maxy = (std::max)(maxy, in.positions[i + 1]);
    maxz = (std::max)(maxz, in.positions[i + 2]);
  }
  const float mesh_span =
      std::hypot(maxx - minx, std::hypot(maxy - miny, maxz - minz));
  out->positions.clear();
  out->indices.clear();
  const size_t tri_count = in.indices.size() / 3;
  out->positions.reserve(in.positions.size());
  out->indices.reserve(in.indices.size());
  for (size_t t = 0; t < tri_count; ++t) {
    const uint32_t i0 = in.indices[t * 3 + 0];
    const uint32_t i1 = in.indices[t * 3 + 1];
    const uint32_t i2 = in.indices[t * 3 + 2];
    const size_t b0 = static_cast<size_t>(i0) * 3u;
    const size_t b1 = static_cast<size_t>(i1) * 3u;
    const size_t b2 = static_cast<size_t>(i2) * 3u;
    if (b0 + 2 >= in.positions.size() || b1 + 2 >= in.positions.size() ||
        b2 + 2 >= in.positions.size()) {
      continue;
    }
    const float mx =
        (in.positions[b0] + in.positions[b1] + in.positions[b2]) / 3.f;
    const float my =
        (in.positions[b0 + 1] + in.positions[b1 + 1] + in.positions[b2 + 1]) /
        3.f;
    const float mz =
        (in.positions[b0 + 2] + in.positions[b1 + 2] + in.positions[b2 + 2]) /
        3.f;
    const float dist = std::hypot(mx - cx, std::hypot(my - cy, mz - cz));
    const int stride =
        terrain_lod_tin_stride_at(camera_distance, dist, mesh_span);
    const int s = stride > 1 ? stride : 1;
    if ((t % static_cast<size_t>(s)) != 0) {
      continue;
    }
    const uint32_t base = static_cast<uint32_t>(out->positions.size() / 3);
    out->positions.push_back(in.positions[b0]);
    out->positions.push_back(in.positions[b0 + 1]);
    out->positions.push_back(in.positions[b0 + 2]);
    out->positions.push_back(in.positions[b1]);
    out->positions.push_back(in.positions[b1 + 1]);
    out->positions.push_back(in.positions[b1 + 2]);
    out->positions.push_back(in.positions[b2]);
    out->positions.push_back(in.positions[b2 + 1]);
    out->positions.push_back(in.positions[b2 + 2]);
    out->indices.push_back(base);
    out->indices.push_back(base + 1);
    out->indices.push_back(base + 2);
  }
  return out->indices.size() >= 3;
}

bool aabb_from_xyz(const std::vector<float>& xyz, double* min_x, double* min_y,
                   double* min_z, double* max_x, double* max_y, double* max_z) {
  if (xyz.size() < 3 || !min_x || !min_y || !min_z || !max_x || !max_y ||
      !max_z) {
    return false;
  }
  float mn_x = xyz[0];
  float mn_y = xyz[1];
  float mn_z = xyz[2];
  float mx_x = mn_x;
  float mx_y = mn_y;
  float mx_z = mn_z;
  for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
    mn_x = (std::min)(mn_x, xyz[i]);
    mn_y = (std::min)(mn_y, xyz[i + 1]);
    mn_z = (std::min)(mn_z, xyz[i + 2]);
    mx_x = (std::max)(mx_x, xyz[i]);
    mx_y = (std::max)(mx_y, xyz[i + 1]);
    mx_z = (std::max)(mx_z, xyz[i + 2]);
  }
  *min_x = mn_x;
  *min_y = mn_y;
  *min_z = mn_z;
  *max_x = mx_x;
  *max_y = mx_y;
  *max_z = mx_z;
  return true;
}

Node* attach_payload_mesh(World* world, const char* name,
                          const std::vector<float>& xyz,
                          const std::vector<uint32_t>& idx, int lod_key,
                          TerrainSource source) {
  if (!world || xyz.size() < 9 || idx.size() < 3) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double min_z = 0;
  double max_x = 0;
  double max_y = 0;
  double max_z = 0;
  if (!aabb_from_xyz(xyz, &min_x, &min_y, &min_z, &max_x, &max_y, &max_z)) {
    return nullptr;
  }
  const char* node_name = (name && name[0]) ? name : "terrain";
  Node* node =
      world->attach_terrain(node_name, min_x, min_y, min_z, max_x, max_y, max_z);
  if (!node) {
    return nullptr;
  }
  if (!world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                               idx.size())) {
    return nullptr;
  }
  stamp_payload_meta(world, node->id, lod_key, source);
  return world->find(node->id);
}

Node* seed_dem_window_node(World* world, const DemRaster& dem, double tminx,
                           double tminy, double tmaxx, double tmaxy, int edge,
                           bool apply_land_mask, int lod_key,
                           TerrainSource source, const char* name) {
  if (!world || dem.empty() || edge < 2) {
    return nullptr;
  }
  std::vector<float> xyz;
  std::vector<uint32_t> idx;
  std::vector<float> uvs;
  {
    base::ElapsedTimer tess_timer;
    const char* sp =
        dem.source_path().empty() ? nullptr : dem.source_path().c_str();
    bool have =
        sp && dem_mesh_cache_try_get(sp, edge, tminx, tminy, tmaxx, tmaxy,
                                     /*windowed=*/true, apply_land_mask, &xyz,
                                     &idx, &uvs);
    if (!have) {
      have = dem.build_mesh_window(tminx, tminy, tmaxx, tmaxy, edge, &xyz,
                                   &idx, &uvs, apply_land_mask) &&
             !xyz.empty() && !idx.empty();
      if (have && sp) {
        dem_mesh_cache_put(sp, edge, tminx, tminy, tmaxx, tmaxy,
                           /*windowed=*/true, apply_land_mask, xyz, idx, uvs);
      }
    }
    note_dem_phase_tess(
        static_cast<int64_t>(tess_timer.elapsed_milliseconds() + 0.5));
    if (!have) {
      return nullptr;
    }
  }
  Node* node = attach_payload_mesh(world, name, xyz, idx, lod_key, source);
  if (!node) {
    return nullptr;
  }
  if (!uvs.empty()) {
    world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
  }
  std::vector<uint8_t> rgba;
  int tw = 0;
  int th = 0;
  if (dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th) && tw > 0 && th > 0) {
    world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                               static_cast<uint32_t>(tw),
                               static_cast<uint32_t>(th));
    apply_elevation_overlay_texture(world, node->id, dem, edge, &rgba, tw, th);
  }
  stamp_payload_meta(world, node->id, lod_key, source);
  return world->find(node->id);
}

void apply_nested_patch_continuity(World* world, Node* node,
                                   const NestedGridTile& tile) {
  if (!world || !node || !node->has_terrain_mesh()) {
    return;
  }
  std::vector<float> xyz = node->terrain.positions;
  std::vector<uint32_t> idx = node->terrain.indices;
  std::vector<float> uvs = node->terrain.uvs;
  float ymin = xyz[1];
  float ymax = ymin;
  for (size_t i = 1; i < xyz.size(); i += 3) {
    ymin = (std::min)(ymin, xyz[i]);
    ymax = (std::max)(ymax, xyz[i]);
  }
  const float span_y = ymax - ymin;
  const float drop =
      span_y > 0.f ? (std::max)(0.08f * span_y, 1.0e-3f) : 0.05f;
  append_terrain_edge_skirts(&xyz, &idx, &uvs, drop);
  if (!world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                               idx.size())) {
    return;
  }
  if (!uvs.empty() && uvs.size() == (xyz.size() / 3) * 2u) {
    world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
  }
  Node* stamped = world->find(node->id);
  if (!stamped || !stamped->has_terrain_mesh()) {
    return;
  }
  const size_t nvert = stamped->terrain.positions.size() / 3;
  stamped->terrain.morph.assign(nvert, tile.morph_weight);
  aabb_from_xyz(stamped->terrain.positions, &stamped->min_x, &stamped->min_y,
                &stamped->min_z, &stamped->max_x, &stamped->max_y,
                &stamped->max_z);
}

}  // namespace detail
}  // namespace vista
