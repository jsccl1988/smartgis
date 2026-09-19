// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/assets/model.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>

#include "gis/assets/model_internal.h"

namespace gis {
namespace {

const char* leaf_name(const char* path) {
  const char* name = path;
  for (const char* p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      name = p + 1;
    }
  }
  return name;
}

std::string ascii_lower(const char* s) {
  std::string out;
  if (!s) {
    return out;
  }
  for (const char* p = s; *p; ++p) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
  }
  return out;
}

bool ends_with(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool is_cube_name(const char* path) {
  if (!path) {
    return false;
  }
  return std::strcmp(path, "cube") == 0 ||
         std::strcmp(leaf_name(path), "cube") == 0;
}

// 3D Tiles payloads: never Assimp, never a fake tileset via load_file.
bool is_tiles_payload_path(const char* path) {
  const std::string leaf = ascii_lower(leaf_name(path));
  return leaf == "tileset.json" || ends_with(leaf, ".json") ||
         ends_with(leaf, ".b3dm") || ends_with(leaf, ".i3dm") ||
         ends_with(leaf, ".pnts") || ends_with(leaf, ".cmpt");
}

}  // namespace

void load_unit_cube(ModelAsset& out) {
  out.name = "cube";
  out.meshes.clear();
  Mesh mesh;
  const float p[] = {
      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f, 0.5f,
      -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f, 0.5f,
  };
  mesh.positions.assign(p, p + 24);
  const uint32_t idx[] = {
      0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
      3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 2, 6, 1, 6, 5,
  };
  mesh.indices.assign(idx, idx + 36);
  out.meshes.push_back(std::move(mesh));
}

bool has_assimp() {
#ifdef SMT_HAS_ASSIMP
  return true;
#else
  return false;
#endif
}

bool has_tinygltf() {
#ifdef SMT_HAS_TINYGLTF
  return true;
#else
  return false;
#endif
}

bool load_file(const char* path, ModelAsset& out) {
  out = ModelAsset();
  if (!path || !*path) {
    return false;
  }
  if (is_cube_name(path)) {
    load_unit_cube(out);
    return true;
  }
  if (is_tiles_payload_path(path)) {
    return false;
  }
  return detail::load_file_assimp(path, out);
}

bool decode_content(const char* uri, const void* data, size_t size,
                    ModelAsset& out) {
  out = ModelAsset();
  if (!data || size == 0) {
    return false;
  }
  return detail::decode_gltf_bytes(uri, data, size, out);
}

bool decode_content_file(const char* path, ModelAsset& out) {
  out = ModelAsset();
  if (!path || !*path) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff len = in.tellg();
  if (len <= 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);
  std::vector<unsigned char> buf(static_cast<size_t>(len));
  in.read(reinterpret_cast<char*>(buf.data()), len);
  if (!in) {
    return false;
  }
  return decode_content(path, buf.data(), buf.size(), out);
}

bool model_aabb(const ModelAsset& asset, double* min_x, double* min_y,
                double* min_z, double* max_x, double* max_y, double* max_z) {
  bool any = false;
  double lo_x = 0;
  double lo_y = 0;
  double lo_z = 0;
  double hi_x = 0;
  double hi_y = 0;
  double hi_z = 0;
  for (const Mesh& mesh : asset.meshes) {
    const size_t n = mesh.positions.size() / 3;
    for (size_t i = 0; i < n; ++i) {
      const double x = mesh.positions[i * 3];
      const double y = mesh.positions[i * 3 + 1];
      const double z = mesh.positions[i * 3 + 2];
      if (!any) {
        lo_x = hi_x = x;
        lo_y = hi_y = y;
        lo_z = hi_z = z;
        any = true;
      } else {
        if (x < lo_x) lo_x = x;
        if (y < lo_y) lo_y = y;
        if (z < lo_z) lo_z = z;
        if (x > hi_x) hi_x = x;
        if (y > hi_y) hi_y = y;
        if (z > hi_z) hi_z = z;
      }
    }
  }
  if (!any) {
    return false;
  }
  if (min_x) *min_x = lo_x;
  if (min_y) *min_y = lo_y;
  if (min_z) *min_z = lo_z;
  if (max_x) *max_x = hi_x;
  if (max_y) *max_y = hi_y;
  if (max_z) *max_z = hi_z;
  return true;
}

bool flatten_meshes(const ModelAsset& in, Mesh& out) {
  out = Mesh();
  uint32_t base = 0;
  for (const Mesh& mesh : in.meshes) {
    out.positions.insert(out.positions.end(), mesh.positions.begin(),
                         mesh.positions.end());
    for (uint32_t index : mesh.indices) {
      out.indices.push_back(index + base);
    }
    base += static_cast<uint32_t>(mesh.positions.size() / 3);
  }
  return !out.positions.empty() && !out.indices.empty();
}

}  // namespace gis
