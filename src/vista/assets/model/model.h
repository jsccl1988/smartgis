// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_ASSETS_MODEL_MODEL_H_
#define VISTA_ASSETS_MODEL_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"

// CPU mesh assets. Assimp loads standalone files; GPU upload lives in render.

namespace vista {

struct Mesh {
  std::vector<float> positions;
  std::vector<uint32_t> indices;
};

struct ModelAsset {
  std::string name;
  std::vector<Mesh> meshes;
};

VISTA_EXPORT void load_unit_cube(ModelAsset& out);

// Assimp when has_assimp; otherwise only the built-in name "cube".
// tileset.json / b3dm / i3dm / pnts / cmpt are never standalone files.
VISTA_EXPORT bool load_file(const char* path, ModelAsset& out);

VISTA_EXPORT bool has_assimp();
VISTA_EXPORT bool has_tinygltf();

// Tile content (glTF / GLB / b3dm). Without tinygltf this is always false.
VISTA_EXPORT bool decode_content(const char* uri, const void* data, size_t size,
                               ModelAsset& out);
VISTA_EXPORT bool decode_content_file(const char* path, ModelAsset& out);

VISTA_EXPORT bool model_aabb(const ModelAsset& asset, double* min_x,
                           double* min_y, double* min_z, double* max_x,
                           double* max_y, double* max_z);
VISTA_EXPORT bool flatten_meshes(const ModelAsset& in, Mesh& out);

}  // namespace vista

#endif  // VISTA_ASSETS_MODEL_MODEL_H_
