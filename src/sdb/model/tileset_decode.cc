// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/model/model_internal.h"

#include <cstdint>
#include <cstring>
#include <string>

#ifdef SMT_HAS_TINYGLTF
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996 4267 4244)
#endif
#include "tiny_gltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

namespace sdb {
namespace model {
namespace detail {
namespace {

#ifdef SMT_HAS_TINYGLTF

std::string ascii_lower(const char* s) {
  std::string out;
  if (!s) {
    return out;
  }
  for (const char* p = s; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    out.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c));
  }
  return out;
}

bool ends_with(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool looks_glb(const unsigned char* data, size_t size) {
  return size >= 4 && std::memcmp(data, "glTF", 4) == 0;
}

bool looks_b3dm(const unsigned char* data, size_t size) {
  return size >= 4 && std::memcmp(data, "b3dm", 4) == 0;
}

bool skip_b3dm(const unsigned char* data, size_t size, const unsigned char** out,
               size_t* out_size) {
  if (!looks_b3dm(data, size) || size < 28) {
    return false;
  }
  auto read_u32 = [&](size_t off) -> uint32_t {
    uint32_t v = 0;
    std::memcpy(&v, data + off, 4);
    return v;
  };
  const uint32_t feature_json = read_u32(12);
  const uint32_t feature_bin = read_u32(16);
  const uint32_t batch_json = read_u32(20);
  const uint32_t batch_bin = read_u32(24);
  size_t offset = 28;
  offset += static_cast<size_t>(feature_json) + feature_bin + batch_json +
            batch_bin;
  if (offset >= size) {
    offset = 28;
  }
  if (offset >= size) {
    return false;
  }
  *out = data + offset;
  *out_size = size - offset;
  return true;
}

bool copy_accessor_floats(const tinygltf::Model& model,
                          const tinygltf::Accessor& acc,
                          std::vector<float>* dst) {
  if (acc.bufferView < 0 ||
      acc.bufferView >= static_cast<int>(model.bufferViews.size())) {
    return false;
  }
  const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
  if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
    return false;
  }
  const tinygltf::Buffer& buf = model.buffers[view.buffer];
  const int32_t elem = tinygltf::GetComponentSizeInBytes(
      static_cast<uint32_t>(acc.componentType));
  const int32_t comps = tinygltf::GetNumComponentsInType(
      static_cast<uint32_t>(acc.type));
  if (elem <= 0 || comps <= 0) {
    return false;
  }
  const size_t stride =
      view.byteStride ? view.byteStride
                      : static_cast<size_t>(elem) * static_cast<size_t>(comps);
  const size_t ncomp = static_cast<size_t>(comps);
  const unsigned char* base =
      buf.data.data() + view.byteOffset + acc.byteOffset;
  dst->reserve(dst->size() + static_cast<size_t>(acc.count) * 3);
  for (size_t i = 0; i < static_cast<size_t>(acc.count); ++i) {
    const unsigned char* p = base + i * stride;
    float xyz[3] = {0, 0, 0};
    for (size_t c = 0; c < ncomp && c < 3; ++c) {
      if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        std::memcpy(&xyz[c], p + c * 4, 4);
      } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
        double v = 0;
        std::memcpy(&v, p + c * 8, 8);
        xyz[c] = static_cast<float>(v);
      } else {
        return false;
      }
    }
    dst->push_back(xyz[0]);
    dst->push_back(xyz[1]);
    dst->push_back(xyz[2]);
  }
  return true;
}

bool copy_accessor_indices(const tinygltf::Model& model,
                           const tinygltf::Accessor& acc,
                           std::vector<uint32_t>* dst) {
  if (acc.bufferView < 0 ||
      acc.bufferView >= static_cast<int>(model.bufferViews.size())) {
    return false;
  }
  const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
  if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
    return false;
  }
  const tinygltf::Buffer& buf = model.buffers[view.buffer];
  const size_t elem =
      static_cast<size_t>(tinygltf::GetComponentSizeInBytes(acc.componentType));
  const size_t stride = view.byteStride ? view.byteStride : elem;
  const unsigned char* base =
      buf.data.data() + view.byteOffset + acc.byteOffset;
  dst->reserve(dst->size() + static_cast<size_t>(acc.count));
  for (size_t i = 0; i < static_cast<size_t>(acc.count); ++i) {
    const unsigned char* p = base + i * stride;
    uint32_t v = 0;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
      std::memcpy(&v, p, 4);
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
      uint16_t s = 0;
      std::memcpy(&s, p, 2);
      v = s;
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
      v = p[0];
    } else {
      return false;
    }
    dst->push_back(v);
  }
  return true;
}

bool extract_model(const tinygltf::Model& gltf, ModelAsset& out) {
  out.meshes.clear();
  for (const tinygltf::Mesh& src : gltf.meshes) {
    for (const tinygltf::Primitive& prim : src.primitives) {
      auto pos_it = prim.attributes.find("POSITION");
      if (pos_it == prim.attributes.end()) {
        continue;
      }
      if (pos_it->second < 0 ||
          pos_it->second >= static_cast<int>(gltf.accessors.size())) {
        continue;
      }
      Mesh mesh;
      if (!copy_accessor_floats(gltf, gltf.accessors[pos_it->second],
                                &mesh.positions)) {
        continue;
      }
      if (prim.indices >= 0 &&
          prim.indices < static_cast<int>(gltf.accessors.size())) {
        if (!copy_accessor_indices(gltf, gltf.accessors[prim.indices],
                                   &mesh.indices)) {
          continue;
        }
      } else {
        const uint32_t n = static_cast<uint32_t>(mesh.positions.size() / 3);
        mesh.indices.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
          mesh.indices[i] = i;
        }
      }
      if (!mesh.positions.empty() && !mesh.indices.empty()) {
        out.meshes.push_back(std::move(mesh));
      }
    }
  }
  return !out.meshes.empty();
}

bool load_tiny_model(const unsigned char* data, size_t size, bool binary,
                     ModelAsset& out) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string err;
  std::string warn;
  bool ok = false;
  if (binary) {
    ok = loader.LoadBinaryFromMemory(&model, &err, &warn, data,
                                     static_cast<unsigned int>(size), "");
  } else {
    ok = loader.LoadASCIIFromString(
        &model, &err, &warn, reinterpret_cast<const char*>(data),
        static_cast<unsigned int>(size), "");
  }
  if (!ok) {
    return false;
  }
  return extract_model(model, out);
}

#endif  // SMT_HAS_TINYGLTF

}  // namespace

bool decode_gltf_bytes(const char* uri, const void* data, size_t size,
                       ModelAsset& out) {
  out = ModelAsset();
#ifndef SMT_HAS_TINYGLTF
  (void)uri;
  (void)data;
  (void)size;
  return false;
#else
  if (!data || size == 0) {
    return false;
  }
  const auto* bytes = static_cast<const unsigned char*>(data);
  const std::string name = ascii_lower(uri);
  if (ends_with(name, ".i3dm") || ends_with(name, ".pnts") ||
      ends_with(name, ".cmpt")) {
    return false;
  }
  const unsigned char* payload = bytes;
  size_t payload_size = size;
  bool binary = looks_glb(bytes, size) || ends_with(name, ".glb");
  if (looks_b3dm(bytes, size) || ends_with(name, ".b3dm")) {
    if (!skip_b3dm(bytes, size, &payload, &payload_size)) {
      return false;
    }
    binary = looks_glb(payload, payload_size);
  } else if (ends_with(name, ".gltf")) {
    binary = false;
  }
  if (!binary && looks_glb(payload, payload_size)) {
    binary = true;
  }
  if (!load_tiny_model(payload, payload_size, binary, out)) {
    out = ModelAsset();
    return false;
  }
  if (uri && *uri) {
    out.name = uri;
  }
  return true;
#endif
}

}  // namespace detail
}  // namespace model
}  // namespace sdb
