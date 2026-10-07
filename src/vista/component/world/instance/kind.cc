// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/instance/kind.h"

#include <string>

#include "vista/assets/model/model.h"
#include "vista/component/world/instance/tessellate.h"

namespace vista {
namespace detail {

bool uses_point_cloud_chunks(const Instance& inst) {
  return inst.kind == vista::NodeKind::kPointCloud &&
         !inst.point_chunks.empty() && !inst.point_positions.empty() &&
         (inst.point_positions.size() % 3) == 0;
}

bool tessellate_instance(const Instance& inst, double world_units_per_pixel,
                         vista::TilesetContentCache* cache,
                         vista::TessMesh* out) {
  if (!out) {
    return false;
  }
  out->positions.clear();
  out->indices.clear();
  bool have = false;
  if (inst.kind == vista::NodeKind::kVectorLayer &&
      (inst.ogr_layer || !inst.geoms.empty())) {
    have = tessellate_vector_instance(inst, world_units_per_pixel, *out);
  } else if (inst.kind == vista::NodeKind::kVectorLayer && inst.tin) {
    have = vista::tessellate_tin(inst.tin, *out);
  } else if (inst.kind == vista::NodeKind::kVectorLayer && inst.grid) {
    have = vista::tessellate_grid(inst.grid, inst.grid_nx, inst.grid_ny, *out);
  } else if (inst.kind == vista::NodeKind::kRasterLayer) {
    have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z, inst.max_x,
                                  inst.max_y, inst.max_z, *out);
  } else if (inst.kind == vista::NodeKind::kModel && inst.model) {
    vista::Mesh flat;
    if (vista::flatten_meshes(*inst.model, flat)) {
      out->positions = flat.positions;
      out->indices = flat.indices;
      have = true;
    }
  } else if (inst.kind == vista::NodeKind::kTerrain) {
    if (inst.terrain.has_mesh()) {
      out->positions = inst.terrain.positions;
      out->indices = inst.terrain.indices;
      have = true;
    } else if (inst.geom_3d) {
      have = vista::tessellate_3d_geometry(inst.geom_3d, *out);
    } else {
      have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, *out);
    }
  } else if (inst.kind == vista::NodeKind::kPointCloud) {
    if (!inst.point_positions.empty() &&
        (inst.point_positions.size() % 3) == 0) {
      have = vista::tessellate_point_cloud(inst.point_positions.data(),
                                           inst.point_positions.size() / 3,
                                           0.07f, *out);
    } else if (inst.geom_3d) {
      have = vista::tessellate_3d_geometry(inst.geom_3d, *out);
    } else {
      have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, *out);
    }
  } else if (inst.kind == vista::NodeKind::kModel && inst.geom_3d) {
    have = vista::tessellate_3d_geometry(inst.geom_3d, *out);
  } else if (inst.kind == vista::NodeKind::kTileset) {
    for (const std::string& uri : inst.visible_uris) {
      if (uri.empty()) {
        continue;
      }
      const vista::ModelAsset* cached_asset = nullptr;
      if (cache) {
        const vista::TilesetContentEntry* entry = cache->try_get(uri);
        if (entry) {
          if (!entry->decode_ok) {
            continue;
          }
          cached_asset = &entry->asset;
        }
      }
      vista::ModelAsset decoded;
      const vista::ModelAsset* asset = cached_asset;
      if (!asset) {
        if (!vista::decode_content_file(uri.c_str(), decoded)) {
          continue;
        }
        asset = &decoded;
      }
      vista::Mesh flat;
      if (!vista::flatten_meshes(*asset, flat) || flat.indices.empty()) {
        continue;
      }
      const uint32_t base = static_cast<uint32_t>(out->positions.size() / 3);
      out->positions.insert(out->positions.end(), flat.positions.begin(),
                            flat.positions.end());
      for (uint32_t idx : flat.indices) {
        out->indices.push_back(base + idx);
      }
      have = true;
    }
    if (!have) {
      have = vista::tessellate_aabb(inst.min_x, inst.min_y, inst.min_z,
                                    inst.max_x, inst.max_y, inst.max_z, *out);
    }
  }
  return have;
}

}  // namespace detail
}  // namespace vista
