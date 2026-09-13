// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_MODEL_MODEL_INTERNAL_H_
#define SDB_MODEL_MODEL_INTERNAL_H_

#include "sdb/model/model.h"

#include <cstddef>

namespace sdb {
namespace model {
namespace detail {

bool load_file_assimp(const char* path, ModelAsset& out);
bool decode_gltf_bytes(const char* uri, const void* data, size_t size,
                       ModelAsset& out);

}  // namespace detail
}  // namespace model
}  // namespace sdb

#endif  // SDB_MODEL_MODEL_INTERNAL_H_
