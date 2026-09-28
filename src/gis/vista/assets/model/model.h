// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ASSETS_MODEL_H_
#define GIS_ASSETS_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gis/gis_export.h"

// CPU mesh assets. Assimp loads standalone files; GPU upload lives in render.

namespace gis {

struct Mesh {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
};

struct ModelAsset {
  std::string name;
  std::vector<Mesh> meshes;
};

GIS_EXPORT void load_unit_cube(ModelAsset& out);

// Assimp when smt_has_assimp; otherwise only the built-in name "cube".
// tileset.json / b3dm / i3dm / pnts / cmpt are never standalone files.
GIS_EXPORT bool load_file(const char* path, ModelAsset& out);

GIS_EXPORT bool has_assimp();
GIS_EXPORT bool has_tinygltf();

// Tile content (glTF / GLB / b3dm). Without tinygltf this is always false.
GIS_EXPORT bool decode_content(const char* uri, const void* data, size_t size,
                               ModelAsset& out);
GIS_EXPORT bool decode_content_file(const char* path, ModelAsset& out);

GIS_EXPORT bool model_aabb(const ModelAsset& asset, double* min_x,
                           double* min_y, double* min_z, double* max_x,
                           double* max_y, double* max_z);
GIS_EXPORT bool flatten_meshes(const ModelAsset& in, Mesh& out);

}  // namespace gis

#endif  // GIS_ASSETS_MODEL_H_
