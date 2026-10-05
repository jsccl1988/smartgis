// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/terrain/pass.h"

#include <cstdio>
#include <memory>
#include <vector>

#include "render/rhi/rhi.h"
#include "vista/component/world/terrain/lod.h"
#include "vista/component/world/world.h"
#include "vista/pass/world/pass.h"

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
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  expect(vista::terrain_lod_max_edge(0.5f) >= vista::terrain_lod_max_edge(5.f),
         "near camera denser raster edge");
  expect(vista::terrain_lod_tin_stride(0.5f) <=
             vista::terrain_lod_tin_stride(5.f),
         "near camera denser TIN stride");
  expect(vista::terrain_lod_surface_edge(1.0f) ==
             vista::terrain_lod_max_edge(1.0f),
         "surface edge aliases raster edge");

  vista::World world;
  vista::Node* node = world.attach_terrain("tin_like", 0, 0, 0, 1, 1, 0.2);
  expect(node != nullptr, "attach terrain");
  const float positions[] = {
      0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.1f, 0.f, 1.f, 0.05f,
  };
  const uint32_t indices[] = {0, 1, 2, 0, 2, 3};
  expect(world.set_terrain_mesh(node->id, positions, 12, indices, 6),
         "set terrain mesh");
  const uint8_t rgba[] = {40, 120, 50, 255, 42, 122, 52, 255,
                          38, 118, 48, 255, 41, 121, 51, 255};
  expect(world.set_terrain_texture(node->id, rgba, 16, 2, 2), "set texture");
  expect(node->terrain.has_mesh(), "payload mesh present");

  vista::WorldPass pass;
  pass.sync_from(world);
  expect(pass.instance_count() == 1, "synced one instance");
  expect(pass.instance_at(0)->terrain.has_mesh(), "instance terrain mesh");
  expect(pass.instance_at(0)->terrain.has_texture(), "instance terrain tex");

  pass.update_solid_terrain(world.generation(), true);
  expect(pass.solid_terrain_forced(), "solid gate on");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");
  expect(pass.record(device.get(), list, 64, 64), "WorldPass record terrain");
  auto* stub = static_cast<StubCommandList*>(list);
  expect(stub != nullptr && stub->draw_indexed_calls >= 1,
         "terrain draw indexed");
  expect(pass.mesh_count() >= 1, "uploaded mesh");

  device->destroy_command_list(list);
  pass.release();

  vista::TerrainPass terrain;
  std::vector<vista::Instance> insts(1);
  insts[0].kind = vista::NodeKind::kTerrain;
  insts[0].terrain = node->terrain;
  terrain.update_solid_terrain(insts, 1, true);
  expect(terrain.solid_terrain_forced(), "TerrainPass gate");
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  terrain.forced_solid_rgb(&r, &g, &b);
  expect(g > r, "olive land tint");

  if (g_fails != 0) {
    std::fprintf(stderr, "terrain_pass_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "terrain_pass_test: ok\n");
  return 0;
}
