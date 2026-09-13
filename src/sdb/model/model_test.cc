// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/model/model.h"
#include "sdb/model/tileset.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void append_u32(std::vector<unsigned char>& b, uint32_t v) {
  b.push_back(static_cast<unsigned char>(v & 0xffu));
  b.push_back(static_cast<unsigned char>((v >> 8) & 0xffu));
  b.push_back(static_cast<unsigned char>((v >> 16) & 0xffu));
  b.push_back(static_cast<unsigned char>((v >> 24) & 0xffu));
}

void pad4(std::vector<unsigned char>& b, unsigned char fill) {
  while (b.size() % 4 != 0) {
    b.push_back(fill);
  }
}

// Minimal TRIANGLES glTF 2.0 GLB: one triangle in the XY plane.
std::vector<unsigned char> make_triangle_glb() {
  const char* json =
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
      "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
      "\"indices\":1}]}],"
      "\"accessors\":["
      "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"max\":[1,1,0],\"min\":[0,0,0]},"
      "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}"
      "],"
      "\"bufferViews\":["
      "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}"
      "],"
      "\"buffers\":[{\"byteLength\":44}]}";

  std::vector<unsigned char> json_chunk(json, json + std::strlen(json));
  pad4(json_chunk, ' ');

  std::vector<unsigned char> bin;
  const float pos[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
  bin.insert(bin.end(), reinterpret_cast<const unsigned char*>(pos),
             reinterpret_cast<const unsigned char*>(pos) + sizeof(pos));
  const uint16_t idx[] = {0, 1, 2};
  bin.insert(bin.end(), reinterpret_cast<const unsigned char*>(idx),
             reinterpret_cast<const unsigned char*>(idx) + sizeof(idx));
  pad4(bin, 0);

  std::vector<unsigned char> out;
  const uint32_t total = static_cast<uint32_t>(12 + 8 + json_chunk.size() + 8 +
                                               bin.size());
  out.insert(out.end(), {'g', 'l', 'T', 'F'});
  append_u32(out, 2);
  append_u32(out, total);
  append_u32(out, static_cast<uint32_t>(json_chunk.size()));
  out.insert(out.end(), {'J', 'S', 'O', 'N'});
  out.insert(out.end(), json_chunk.begin(), json_chunk.end());
  append_u32(out, static_cast<uint32_t>(bin.size()));
  out.insert(out.end(), {'B', 'I', 'N', 0});
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

std::vector<unsigned char> wrap_b3dm(const std::vector<unsigned char>& glb) {
  std::vector<unsigned char> out;
  out.insert(out.end(), {'b', '3', 'd', 'm'});
  append_u32(out, 1);
  append_u32(out, static_cast<uint32_t>(28 + glb.size()));
  append_u32(out, 0);
  append_u32(out, 0);
  append_u32(out, 0);
  append_u32(out, 0);
  out.insert(out.end(), glb.begin(), glb.end());
  return out;
}

}  // namespace

int main() {
  expect(!sdb::model::has_assimp(), "assimp not pinned locally");

  sdb::model::ModelAsset cube;
  sdb::model::load_unit_cube(cube);
  expect(cube.meshes.size() == 1, "cube mesh count");
  expect(cube.meshes[0].positions.size() == 24, "8 verts");
  expect(cube.meshes[0].indices.size() == 36, "36 indices");

  double min_x = 0, min_y = 0, min_z = 0, max_x = 0, max_y = 0, max_z = 0;
  expect(sdb::model::model_aabb(cube, &min_x, &min_y, &min_z, &max_x, &max_y,
                               &max_z),
         "cube aabb");
  expect(min_x == -0.5 && max_x == 0.5, "cube x");

  sdb::model::Mesh flat;
  expect(sdb::model::flatten_meshes(cube, flat), "flatten cube");
  expect(flat.positions.size() == 24 && flat.indices.size() == 36, "flat size");

  sdb::model::ModelAsset from_name;
  expect(sdb::model::load_file("cube", from_name), "load cube name");
  expect(from_name.meshes.size() == 1, "cube via load_file");

  sdb::model::ModelAsset missing;
  expect(!sdb::model::load_file("missing.obj", missing), "missing file");
  expect(missing.meshes.empty(), "missing empty");

  sdb::model::ModelAsset tileset_as_file;
  expect(!sdb::model::load_file("tileset.json", tileset_as_file),
         "tileset.json is not a standalone model");
  expect(!sdb::model::load_file("tile.b3dm", tileset_as_file),
         "b3dm is not Assimp");

  const char* json =
      "{\"asset\":{\"version\":\"1.1\"},\"root\":{"
      "\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":100,\"refine\":\"REPLACE\","
      "\"content\":{\"uri\":\"root.glb\"},"
      "\"children\":[{\"boundingVolume\":{\"box\":[10,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"child.glb\"}}]}}";

  sdb::model::Tileset tileset;
  expect(sdb::model::parse_tileset_json(json, std::strlen(json), tileset),
         "parse tileset");
  expect(tileset.root.children.size() == 1, "one child");
  expect(tileset.root.content_uri == "root.glb", "root uri");
  expect(tileset.root.children[0].content_uri == "child.glb", "child uri");

  sdb::model::ViewState view;
  view.eye_x = 0;
  view.eye_y = 0;
  view.eye_z = 10;
  view.sse_denominator = 1;

  std::vector<const sdb::model::Tile*> visible;
  sdb::model::select_tiles(tileset, view, 1e9, visible);
  expect(visible.size() == 1, "huge sse: root only");
  expect(visible[0] == &tileset.root, "root selected");

  visible.clear();
  sdb::model::select_tiles(tileset, view, 0, visible);
  expect(visible.size() == 1, "small sse: one child");
  expect(visible[0] == &tileset.root.children[0], "REPLACE omits parent");

  const char* add_json =
      "{\"asset\":{\"version\":\"1.0\"},\"root\":{"
      "\"boundingVolume\":{\"region\":[-1,-1,-1,1,1,1]},"
      "\"geometricError\":50,\"refine\":\"ADD\","
      "\"content\":{\"uri\":\"add-root.glb\"},"
      "\"children\":[{\"boundingVolume\":{\"region\":[0,0,0,1,1,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"add-child.glb\"}}]}}";
  sdb::model::Tileset add_set;
  expect(sdb::model::parse_tileset_json(add_json, std::strlen(add_json), add_set),
         "parse ADD tileset");
  expect(add_set.root.refine == sdb::model::Refine::kAdd, "ADD refine");
  visible.clear();
  sdb::model::select_tiles(add_set, view, 0, visible);
  expect(visible.size() == 2, "ADD emits parent and child");
  expect(visible[0] == &add_set.root && visible[1] == &add_set.root.children[0],
         "ADD order");

  const char* gltf_json =
      "{\"asset\":{\"version\":\"2.0\"},\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}]}";
  sdb::model::Tileset not_tiles;
  expect(!sdb::model::parse_tileset_json(gltf_json, std::strlen(gltf_json),
                                        not_tiles),
         "glTF JSON is not a tileset");

  sdb::model::Tileset bad;
  expect(!sdb::model::parse_tileset_json("{}", 2, bad), "empty object");

  const std::vector<unsigned char> glb = make_triangle_glb();
  sdb::model::ModelAsset decoded;
  const bool got = sdb::model::decode_content("tri.glb", glb.data(), glb.size(),
                                             decoded);
  if (sdb::model::has_tinygltf()) {
    expect(got, "decode triangle glb");
    expect(decoded.meshes.size() == 1, "one decoded mesh");
    expect(decoded.meshes[0].positions.size() == 9, "3 verts");
    expect(decoded.meshes[0].indices.size() == 3, "3 indices");
    const std::vector<unsigned char> b3dm = wrap_b3dm(glb);
    sdb::model::ModelAsset from_b3dm;
    expect(sdb::model::decode_content("tile.b3dm", b3dm.data(), b3dm.size(),
                                     from_b3dm),
           "decode b3dm");
    expect(from_b3dm.meshes.size() == 1, "b3dm mesh");
  } else {
    expect(!got, "decode_content false without tinygltf");
  }

  sdb::model::ModelAsset empty_dec;
  expect(!sdb::model::decode_content("x.glb", nullptr, 0, empty_dec),
         "empty payload");

  {
    const char* obj_path = "model_test_tri.obj";
    std::ofstream obj(obj_path);
    obj << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    obj.close();
    sdb::model::ModelAsset obj_asset;
    const bool obj_ok = sdb::model::load_file(obj_path, obj_asset);
    if (sdb::model::has_assimp()) {
      expect(obj_ok && obj_asset.meshes.size() == 1, "assimp obj");
    } else {
      expect(!obj_ok && obj_asset.meshes.empty(), "obj needs assimp");
    }
    std::remove(obj_path);
  }

  if (g_fails) {
    std::fprintf(stderr, "model_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "model_test: ok (assimp=%d tinygltf=%d)\n",
               sdb::model::has_assimp() ? 1 : 0,
               sdb::model::has_tinygltf() ? 1 : 0);
  return 0;
}
