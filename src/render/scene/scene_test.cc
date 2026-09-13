// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/scene.h"
#include "render/rhi/rhi.h"
#include "sdb/model/model.h"
#include "sdb/model/tileset.h"
#include "sdb/scene/scene.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
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

}  // namespace

int main() {
  // Null / GpuScene sync only. Real FlyCube init/present hangs headless;
  // exercise that path under rhi_test with SMT_RUN_FLYCUBE_GPU=1.
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  sdb::scene::World world;
  world.add_node(sdb::scene::NodeKind::kVectorLayer, "roads", 0, 0, 0, 1, 1, 0);
  world.add_node(sdb::scene::NodeKind::kModel, "cube", 0, 0, 0, 1, 1, 1);

  std::unique_ptr<render::rhi::Device> device(
      render::rhi::create_device(render::rhi::Backend::kNull));

  render::scene::GpuScene gpu;
  gpu.sync_from(world);
  expect(gpu.instance_count() == 2, "two instances");
  expect(gpu.instance_at(0)->kind == sdb::scene::NodeKind::kVectorLayer, "2d");
  expect(gpu.instance_at(1)->kind == sdb::scene::NodeKind::kModel, "3d");

  sdb::model::ModelAsset cube;
  sdb::model::load_unit_cube(cube);
  sdb::scene::World models;
  expect(models.attach_model(&cube, "cube") != nullptr, "attach model asset");
  render::scene::GpuScene model_gpu;
  model_gpu.sync_from(models);
  expect(model_gpu.instance_count() == 1, "model instance");
  expect(model_gpu.instance_at(0)->model == &cube, "model pointer synced");
  model_gpu.set_solid_color(1.f, 0.f, 0.f, 1.f);
  model_gpu.set_view_ortho(0, 0, 10, 10);
  expect(model_gpu.has_view_ortho(), "view ortho set");
  expect(device->initialize(render::rhi::DeviceDesc()), "null device");
  // Fresh lists per record; NullDevice destroy_* intentionally leaks stubs
  // (FlyCube-linked CRT can hang on operator delete).
  render::rhi::CommandList* model_list = device->create_command_list();
  expect(model_gpu.record(device.get(), model_list, 64, 64), "record cube");
  auto* model_stub =
      static_cast<render::rhi::StubCommandList*>(model_list);
  expect(model_stub->draw_indexed_calls >= 1, "cube draw");
  expect(model_stub->set_solid_color_calls >= 1, "solid color on cube");
  expect(model_stub->index_counts.size() >= 1 &&
             model_stub->index_counts[0] == 36,
         "unit cube 36 indices");

  const uint64_t gen = gpu.synced_generation();
  gpu.sync_from(world);
  expect(gpu.synced_generation() == gen, "second sync no-op");
  expect(gpu.instance_count() == 2, "count unchanged");
  render::rhi::CommandList* list = device->create_command_list();
  expect(gpu.record(device.get(), list, 64, 64), "record");
  expect(device->execute(list), "execute");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  expect(stub->closed, "closed after record");

  expect(!gpu.record(device.get(), nullptr, 64, 64), "null list");
  render::rhi::CommandList* list2 = device->create_command_list();
  expect(!gpu.record(device.get(), list2, 0, 64), "zero width");

  // Tileset content → mesh: missing URI keeps AABB (36 indices). With tinygltf
  // and a temp GLB, decoded triangle (3 indices) replaces the AABB bridge.
  {
    const char* ts_json =
        "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
        "\"geometricError\":1,\"content\":{\"uri\":\"missing.glb\"}}}";
    sdb::model::Tileset tileset;
    expect(sdb::model::parse_tileset_json(ts_json, std::strlen(ts_json),
                                          tileset),
           "tileset parse for gpu");
    sdb::scene::World ts_world;
    sdb::scene::Node* ts_node = ts_world.attach_tileset(&tileset, "ts");
    expect(ts_node != nullptr, "attach tileset gpu");
    std::vector<const sdb::model::Tile*> vis;
    vis.push_back(&tileset.root);
    expect(ts_world.apply_tileset_selection(ts_node->id, vis), "select uri");
    render::scene::GpuScene ts_gpu;
    ts_gpu.sync_from(ts_world);
    expect(ts_gpu.instance_count() == 1, "tileset instance");
    expect(ts_gpu.instance_at(0)->visible_uris.size() == 1, "visible uri");
    render::rhi::CommandList* ts_list = device->create_command_list();
    expect(ts_gpu.record(device.get(), ts_list, 64, 64), "record tileset aabb");
    auto* ts_stub = static_cast<render::rhi::StubCommandList*>(ts_list);
    expect(ts_stub->index_counts.size() >= 1 && ts_stub->index_counts[0] == 36,
           "missing content uses AABB 36 indices");

    if (sdb::model::has_tinygltf()) {
      const char* glb_path = "scene_gpu_tile.glb";
      // Minimal TRIANGLES glTF 2.0 GLB (same fixture shape as model_test).
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
      std::string json_pad(json);
      while (json_pad.size() % 4 != 0) {
        json_pad.push_back(' ');
      }
      std::vector<unsigned char> bin(44, 0);
      const float pos[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
      std::memcpy(bin.data(), pos, sizeof(pos));
      const uint16_t idx[] = {0, 1, 2};
      std::memcpy(bin.data() + 36, idx, sizeof(idx));
      std::ofstream out(glb_path, std::ios::binary);
      const uint32_t total =
          static_cast<uint32_t>(12 + 8 + json_pad.size() + 8 + bin.size());
      auto write_u32 = [&out](uint32_t v) {
        const unsigned char b[4] = {
            static_cast<unsigned char>(v & 0xffu),
            static_cast<unsigned char>((v >> 8) & 0xffu),
            static_cast<unsigned char>((v >> 16) & 0xffu),
            static_cast<unsigned char>((v >> 24) & 0xffu)};
        out.write(reinterpret_cast<const char*>(b), 4);
      };
      out.write("glTF", 4);
      write_u32(2);
      write_u32(total);
      write_u32(static_cast<uint32_t>(json_pad.size()));
      out.write("JSON", 4);
      out.write(json_pad.data(), static_cast<std::streamsize>(json_pad.size()));
      write_u32(static_cast<uint32_t>(bin.size()));
      out.write("BIN\0", 4);
      out.write(reinterpret_cast<const char*>(bin.data()),
                static_cast<std::streamsize>(bin.size()));
      out.close();

      const char* ts_json2 =
          "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
          "\"geometricError\":1,\"content\":{\"uri\":\"scene_gpu_tile.glb\"}}}";
      sdb::model::Tileset tileset2;
      expect(sdb::model::parse_tileset_json(ts_json2, std::strlen(ts_json2),
                                            tileset2),
             "tileset parse glb");
      sdb::scene::World ts_world2;
      sdb::scene::Node* n2 = ts_world2.attach_tileset(&tileset2, "ts2");
      std::vector<const sdb::model::Tile*> vis2;
      vis2.push_back(&tileset2.root);
      expect(ts_world2.apply_tileset_selection(n2->id, vis2), "select glb");
      render::scene::GpuScene ts_gpu2;
      ts_gpu2.sync_from(ts_world2);
      render::rhi::CommandList* list3 = device->create_command_list();
      expect(ts_gpu2.record(device.get(), list3, 64, 64), "record decoded tile");
      auto* stub3 = static_cast<render::rhi::StubCommandList*>(list3);
      expect(stub3->index_counts.size() >= 1 && stub3->index_counts[0] == 3,
             "decoded tile triangle 3 indices");
      std::remove(glb_path);
    }
  }

  device->shutdown();

  if (g_fails) {
    std::fprintf(stderr, "scene_gpu_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_gpu_test: ok\n");
  return 0;
}
