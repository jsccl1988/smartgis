// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/scene.h"
#include "render/rhi/rhi.h"
#include "sdb/scene/scene.h"

#include <cstdio>
#include <memory>

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

  const uint64_t gen = gpu.synced_generation();
  gpu.sync_from(world);
  expect(gpu.synced_generation() == gen, "second sync no-op");
  expect(gpu.instance_count() == 2, "count unchanged");
  expect(device->initialize(render::rhi::DeviceDesc()), "null device");
  render::rhi::CommandList* list = device->create_command_list();
  expect(gpu.record(device.get(), list, 64, 64), "record");
  expect(device->execute(list), "execute");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  expect(stub->closed, "closed after record");
  device->destroy_command_list(list);

  expect(!gpu.record(device.get(), nullptr, 64, 64), "null list");
  render::rhi::CommandList* list2 = device->create_command_list();
  expect(!gpu.record(device.get(), list2, 0, 64), "zero width");
  device->destroy_command_list(list2);
  gpu.release();

  if (g_fails) {
    std::fprintf(stderr, "scene_gpu_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_gpu_test: ok\n");
  return 0;
}
