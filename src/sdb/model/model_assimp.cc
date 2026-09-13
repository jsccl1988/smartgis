// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/model/model_internal.h"

#ifdef SMT_HAS_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif

namespace sdb {
namespace model {
namespace detail {

#ifdef SMT_HAS_ASSIMP

bool load_file_assimp(const char* path, ModelAsset& out) {
  out = ModelAsset();
  if (!path || !*path) {
    return false;
  }
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(
      path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
  if (!scene || !scene->HasMeshes()) {
    return false;
  }
  if (scene->mRootNode && scene->mRootNode->mName.length > 0) {
    out.name = scene->mRootNode->mName.C_Str();
  } else {
    out.name = path;
  }
  for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
    const aiMesh* src = scene->mMeshes[m];
    if (!src || !src->HasPositions()) {
      continue;
    }
    Mesh mesh;
    mesh.positions.reserve(static_cast<size_t>(src->mNumVertices) * 3);
    for (unsigned int v = 0; v < src->mNumVertices; ++v) {
      mesh.positions.push_back(src->mVertices[v].x);
      mesh.positions.push_back(src->mVertices[v].y);
      mesh.positions.push_back(src->mVertices[v].z);
    }
    for (unsigned int f = 0; f < src->mNumFaces; ++f) {
      const aiFace& face = src->mFaces[f];
      if (face.mNumIndices < 3) {
        continue;
      }
      for (unsigned int i = 1; i + 1 < face.mNumIndices; ++i) {
        mesh.indices.push_back(face.mIndices[0]);
        mesh.indices.push_back(face.mIndices[i]);
        mesh.indices.push_back(face.mIndices[i + 1]);
      }
    }
    if (!mesh.positions.empty() && !mesh.indices.empty()) {
      out.meshes.push_back(std::move(mesh));
    }
  }
  if (out.meshes.empty()) {
    out = ModelAsset();
    return false;
  }
  return true;
}

#else

bool load_file_assimp(const char* path, ModelAsset& out) {
  (void)path;
  out = ModelAsset();
  return false;
}

#endif

}  // namespace detail
}  // namespace model
}  // namespace sdb
