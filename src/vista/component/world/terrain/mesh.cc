// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "base/time/elapsed_timer.h"
#include "vista/component/world/terrain/seed.h"
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

bool apply_terrain_albedo_texture(World* world, uint64_t node_id,
                                  const DemRaster& dem, int edge) {
  if (!world || dem.empty() || edge < 2) {
    return false;
  }
  std::vector<uint8_t> rgba;
  int tw = 0;
  int th = 0;
  if (dem.bake_map_drape_rgba(edge, &rgba, &tw, &th) && tw > 0 && th > 0) {
    world->set_terrain_texture(node_id, rgba.data(), rgba.size(),
                               static_cast<uint32_t>(tw),
                               static_cast<uint32_t>(th));
    return true;
  }
  if (!dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th) || tw < 2 || th < 2) {
    return false;
  }
  world->set_terrain_texture(node_id, rgba.data(), rgba.size(),
                             static_cast<uint32_t>(tw),
                             static_cast<uint32_t>(th));
  apply_elevation_overlay_texture(world, node_id, dem, edge, &rgba, tw, th);
  return true;
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
  apply_terrain_albedo_texture(world, node->id, dem, edge);
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

namespace {

void clear_terrain_nodes(World* world) {
  while (true) {
    bool removed = false;
    for (size_t i = 0; i < world->node_count(); ++i) {
      const Node* n = world->node_at(i);
      if (n && n->kind == NodeKind::kTerrain) {
        if (world->remove_node(n->id)) {
          removed = true;
          break;
        }
      }
    }
    if (!removed) {
      break;
    }
  }
}

bool attach_view_seed(World* world, const DemViewSeed& seed, const char* name,
                      std::vector<float>* xyz, std::vector<unsigned>* idx) {
  Node* node = world->attach_terrain(
      name, static_cast<double>(seed.min_x), static_cast<double>(seed.min_y),
      static_cast<double>(seed.min_z), static_cast<double>(seed.max_x),
      static_cast<double>(seed.max_y), static_cast<double>(seed.max_z));
  if (!node) {
    return false;
  }
  if (!world->set_terrain_mesh(node->id, seed.xyz.data(), seed.xyz.size(),
                               seed.indices.data(), seed.indices.size())) {
    return false;
  }
  if (!seed.uvs.empty() && seed.uvs.size() == (seed.xyz.size() / 3) * 2) {
    world->set_terrain_uvs(node->id, seed.uvs.data(), seed.uvs.size());
  }
  if (!seed.rgba.empty() && seed.tex_w > 0 && seed.tex_h > 0) {
    world->set_terrain_texture(node->id, seed.rgba.data(), seed.rgba.size(),
                               seed.tex_w, seed.tex_h);
  }
  node->min_x = seed.min_x;
  node->min_y = seed.min_y;
  node->min_z = seed.min_z;
  node->max_x = seed.max_x;
  node->max_y = seed.max_y;
  node->max_z = seed.max_z;
  xyz->assign(seed.xyz.begin(), seed.xyz.end());
  idx->assign(seed.indices.begin(), seed.indices.end());
  return !xyz->empty() && !idx->empty();
}

// Regional windows (plugin world3d AOI, coast pads) must crop the raster to
// the lon/lat frame. Seeding full-China DEM into a tight frame overscales the
// mesh (±12 orbit units vs camera span 3.2) and paints black silhouettes.
Node* seed_view_or_china(World* world, double xmin, double ymin, double xmax,
                         double ymax, float orbit_distance, int next_lod) {
  DemRaster dem;
  const std::string path = find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    // Real-data policy: no synthetic China DEM stand-in.
    return nullptr;
  }
  const double span_lon = xmax - xmin;
  const double span_lat = ymax - ymin;
  const bool regional =
      span_lon < 50.0 || span_lat < 30.0;  // full China is ~62° × 36°
  // Close orbit → multi-tile; any regional frame → crop even when far.
  if (orbit_distance < 2.4f || regional) {
    // Budget matches DemRaster default: one dense china_dem national tile
    // (~512×320) plus headroom for 2×2 regional crops.
    const size_t tiles = seed_dem_view_tiles_into_world(
        world, dem, xmin, ymin, xmax, ymax, orbit_distance, 196608, "views_dem");
    if (tiles > 0) {
      for (size_t i = 0; i < world->node_count(); ++i) {
        const Node* at = world->node_at(i);
        if (!at) {
          continue;
        }
        Node* n = world->find(at->id);
        if (n && n->kind == NodeKind::kTerrain && n->has_terrain_mesh()) {
          return n;
        }
      }
    }
  }
  return seed_china_dem_into_world(world, nullptr, 0, "views_dem", next_lod);
}

}  // namespace

DemViewMeshResult rebuild_dem_view_mesh(World* world, double xmin, double ymin,
                                        double xmax, double ymax,
                                        float orbit_distance,
                                        std::vector<float>* xyz,
                                        std::vector<unsigned>* idx,
                                        const DemViewMeshHooks* hooks) {
  DemViewMeshResult result;
  if (!world || !xyz || !idx) {
    return result;
  }
  result.cache_key = dem_seed_cache_key(orbit_distance);
  const int next_lod = DemRaster::lod_max_edge(orbit_distance);

  const std::string dem_path = find_sample_dem_path();
  if (!dem_path.empty()) {
    DemViewSeed seed;
    if (dem_view_seed_cache_try_get(dem_path.c_str(), result.cache_key, xmin,
                                    ymin, xmax, ymax, &seed)) {
      clear_terrain_nodes(world);
      xyz->clear();
      idx->clear();
      if (attach_view_seed(world, seed, "views_dem", xyz, idx)) {
        result.painted = true;
        result.seed_cache_hit = true;
        result.elev_cy = seed.elev_cy;
        // Attribute cold DEM phases as hits (seed blob covers load/tess/hypso).
        note_dem_phase_load(0, /*cache_hit=*/true);
        note_dem_phase_tess(0);
        note_dem_phase_hypso(0, /*cache_hit=*/true);
        return result;
      }
    }
  }

  clear_terrain_nodes(world);
  xyz->clear();
  idx->clear();
  Node* node =
      seed_view_or_china(world, xmin, ymin, xmax, ymax, orbit_distance, next_lod);
  if (!node || !node->has_terrain_mesh()) {
    return result;
  }

  // Prefer concatenating all terrain tiles into the paint buffer.
  xyz->clear();
  idx->clear();
  std::vector<uint64_t> ids;
  for (size_t i = 0; i < world->node_count(); ++i) {
    const Node* n = world->node_at(i);
    if (n && n->kind == NodeKind::kTerrain && n->has_terrain_mesh()) {
      ids.push_back(n->id);
    }
  }
  if (ids.empty()) {
    return result;
  }
  // Elev center from first tile; normalize each tile in place (no deep copies).
  float elev_cy = 0.f;
  {
    Node* first = world->find(ids[0]);
    if (first && hooks && hooks->capture_elev_center) {
      elev_cy = hooks->capture_elev_center(first->terrain.positions, hooks->ctx);
    }
  }
  size_t total_verts = 0;
  size_t total_idx = 0;
  for (uint64_t id : ids) {
    Node* n = world->find(id);
    if (!n || !n->has_terrain_mesh()) {
      continue;
    }
    total_verts += n->terrain.positions.size() / 3;
    total_idx += n->terrain.indices.size();
  }
  xyz->reserve(total_verts * 3);
  idx->reserve(total_idx);
  for (uint64_t id : ids) {
    Node* n = world->find(id);
    if (!n || !n->has_terrain_mesh()) {
      continue;
    }
    if (hooks && hooks->normalize_xyz) {
      hooks->normalize_xyz(&n->terrain.positions, hooks->ctx);
    }
    detail::aabb_from_xyz(n->terrain.positions, &n->min_x, &n->min_y, &n->min_z,
                          &n->max_x, &n->max_y, &n->max_z);
    const size_t base = xyz->size() / 3;
    xyz->insert(xyz->end(), n->terrain.positions.begin(),
                n->terrain.positions.end());
    for (uint32_t tri : n->terrain.indices) {
      idx->push_back(static_cast<unsigned>(base + tri));
    }
  }
  if (!xyz->empty() && !idx->empty()) {
    result.painted = true;
    result.elev_cy = elev_cy;
    if (!dem_path.empty() && ids.size() == 1) {
      Node* n = world->find(ids[0]);
      if (n && n->has_terrain_mesh()) {
        DemViewSeed seed;
        seed.elev_cy = elev_cy;
        seed.min_x = static_cast<float>(n->min_x);
        seed.min_y = static_cast<float>(n->min_y);
        seed.min_z = static_cast<float>(n->min_z);
        seed.max_x = static_cast<float>(n->max_x);
        seed.max_y = static_cast<float>(n->max_y);
        seed.max_z = static_cast<float>(n->max_z);
        seed.tex_w = n->terrain.tex_w;
        seed.tex_h = n->terrain.tex_h;
        seed.xyz = n->terrain.positions;
        seed.indices = n->terrain.indices;
        seed.uvs = n->terrain.uvs;
        seed.rgba = n->terrain.rgba;
        dem_view_seed_cache_put(dem_path.c_str(), result.cache_key, xmin, ymin,
                                xmax, ymax, seed);
      }
    }
  }
  return result;
}

}  // namespace vista
