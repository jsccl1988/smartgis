// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ASSETS_MODEL_INTERNAL_H_
#define GIS_ASSETS_MODEL_INTERNAL_H_

#include <cstddef>

#include "vista/assets/model/model.h"

namespace vista {
namespace detail {

bool load_file_assimp(const char* path, ModelAsset& out);
bool decode_gltf_bytes(const char* uri, const void* data, size_t size,
                       ModelAsset& out);

}  // namespace detail
}  // namespace vista

#endif  // GIS_ASSETS_MODEL_INTERNAL_H_
