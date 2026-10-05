// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"
#include "vista/pass/world/pass.h"
#include "vista/component/world/world.h"
#include "vista/mesh/tessellate.h"

#include "gis/geo/ops/indexed_tin.h"
#include "gis/map/layer_kind.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "base/process/switches.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

int count_index(const std::vector<uint32_t>& counts, uint32_t n) {
  int c = 0;
  for (uint32_t v : counts) {
    if (v == n) {
      ++c;
    }
  }
  return c;
}

}  // namespace

int main() {
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;

  // Unbuffered so abort/hang still leaves a breadcrumb (matches scene_gpu_test).
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  OGRPoint pt(1, 2);
  OGRLineString line;
  line.addPoint(0, 0);
  line.addPoint(10, 0);
  OGRLinearRing ring;
  ring.addPoint(0, 0);
  ring.addPoint(2, 0);
  ring.addPoint(1, 2);
  ring.closeRings();
  OGRPolygon poly;
  poly.addRing(&ring);

  const OGRGeometry* geoms[] = {&pt, &line, &poly};
  vista::TessMesh expect_2d;
  expect(vista::tessellate_geoms(geoms, 3, expect_2d),
         "tessellate point+line+poly");
  expect(expect_2d.indices.size() >= 12, "2d feature index count");

  OGRTriangulatedSurface surf;
  OGRPoint p0(0, 0, 0);
  OGRPoint p1(1, 0, 0);
  OGRPoint p2(1, 1, 0);
  OGRPoint p3(0, 1, 0);
  expect(geo::add_patch(&surf, p0, p1, p2), "surf tri 0");
  expect(geo::add_patch(&surf, p0, p2, p3), "surf tri 1");
  vista::TessMesh expect_3d;
  expect(vista::tessellate_3d_surface(&surf, expect_3d), "tess 3d surf");
  expect(expect_3d.indices.size() == 6, "3d quad is two triangles");

  OGRLinearRing ring3d;
  ring3d.addPoint(0, 0, 0);
  ring3d.addPoint(1, 0, 0);
  ring3d.addPoint(1, 1, 0);
  ring3d.addPoint(0, 1, 0);
  ring3d.addPoint(0, 0, 0);
  ring3d.closeRings();
  OGRPolygon poly3d;
  poly3d.addRing(&ring3d);

  vista::World world;
  expect(world.attach_vector_geoms("roads", geoms, 3) != nullptr,
         "attach 2d GIS geoms");
  expect(world.attach_3d_geometry(&poly3d, "surface") != nullptr,
         "attach 3d GIS surface");
  expect(world.node_count() == 2, "vector + 3d");
  expect(world.node_at(0)->kind == vista::NodeKind::kVectorLayer,
         "vector kind");
  expect(world.node_at(1)->geom_3d == &poly3d, "3d pointer");

  OGRLineString arc;
  arc.addPoint(0, 0);
  arc.addPoint(1, 1);
  arc.addPoint(2, 0);
  vista::TessMesh expect_arc;
  expect(vista::tessellate_arc(&arc, expect_arc), "tess arc");
  expect(expect_arc.indices.size() == 12, "arc index count");
  const OGRGeometry* arc_geoms[] = {&arc};
  expect(world.attach_vector_geoms("arc", arc_geoms, 1) != nullptr,
         "attach arc");

  OGRLinearRing fan_ring;
  fan_ring.addPoint(1, 1);
  fan_ring.addPoint(3, 0);
  fan_ring.addPoint(0, 0);
  fan_ring.closeRings();
  OGRPolygon fan;
  fan.addRing(&fan_ring);
  vista::TessMesh expect_fan;
  expect(vista::tessellate_fan(&fan, expect_fan), "tess fan");
  expect(expect_fan.indices.size() == 3, "fan index count");
  const OGRGeometry* fan_geoms[] = {&fan};
  expect(world.attach_vector_geoms("fan", fan_geoms, 1) != nullptr,
         "attach fan");

  OGRTriangulatedSurface tin;
  OGRPoint ta(0, 0, 0);
  OGRPoint tb(2, 0, 0);
  OGRPoint tc(1, 2, 0);
  expect(geo::add_patch(&tin, ta, tb, tc), "tin patch");
  vista::TessMesh expect_tin;
  expect(vista::tessellate_tin(&tin, expect_tin), "tess tin");
  expect(expect_tin.indices.size() == 3, "tin index count");
  expect(world.attach_tin(&tin, "tin") != nullptr, "attach tin");

  OGRMultiPoint grid_mp;
  OGRPoint gp00(0, 0);
  OGRPoint gp10(1, 0);
  OGRPoint gp01(0, 1);
  OGRPoint gp11(1, 1);
  grid_mp.addGeometry(&gp00);
  grid_mp.addGeometry(&gp10);
  grid_mp.addGeometry(&gp01);
  grid_mp.addGeometry(&gp11);
  vista::TessMesh expect_grid;
  expect(vista::tessellate_grid(&grid_mp, 2, 2, expect_grid), "tess grid");
  expect(expect_grid.indices.size() == 6, "grid index count");
  expect(world.attach_grid(&grid_mp, 2, 2, "grid") != nullptr, "attach grid");

  vista::TessMesh expect_ras;
  expect(vista::tessellate_aabb(0, 0, 0, 8, 6, 0, expect_ras), "tess raster aabb");
  expect(world.add_node(vista::NodeKind::kRasterLayer, "ras", 0, 0, 0, 8, 6, 0) !=
             nullptr,
         "attach raster aabb");

  vista::TessMesh expect_tiles;
  expect(vista::tessellate_aabb(0, 0, 0, 4, 2, 0, expect_tiles),
         "tess tile aabb");
  expect(world.add_node(vista::NodeKind::kRasterLayer, "tiles", 0, 0, 0, 4, 2,
                        0) != nullptr,
         "attach tile aabb");
  expect(world.node_count() == 8, "mixed remaining GIS nodes");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device && device->initialize(DeviceDesc()), "null initialize");

  vista::WorldPass gpu;
  gpu.sync_from(world);
  expect(gpu.instance_count() == 8, "mixed instance count");

  render::rhi::CommandList* list = device->create_command_list();
  expect(gpu.record(device.get(), list, 64, 64), "record mixed GIS world");
  expect(device->execute(list), "null execute");
  device->present();

  auto* stub = static_cast<StubCommandList*>(list);
  expect(stub->closed, "list closed");
  expect(stub->draw_indexed_calls >= 8, "remaining GIS on one list");
  expect(stub->bind_vertex_calls >= 8, "vertex binds");
  expect(stub->bind_index_calls >= 8, "index binds");
  expect(stub->bind_camera_calls >= 2, "ortho 2D + perspective 3D");

  bool saw_2d = false;
  bool saw_3d = false;
  const uint32_t n2 = static_cast<uint32_t>(expect_2d.indices.size());
  const uint32_t n3 = static_cast<uint32_t>(expect_3d.indices.size());
  const uint32_t n_arc = static_cast<uint32_t>(expect_arc.indices.size());
  const uint32_t n_fan = static_cast<uint32_t>(expect_fan.indices.size());
  const uint32_t n_tin = static_cast<uint32_t>(expect_tin.indices.size());
  const uint32_t n_grid = static_cast<uint32_t>(expect_grid.indices.size());
  const uint32_t n_ras = static_cast<uint32_t>(expect_ras.indices.size());
  const uint32_t n_tiles = static_cast<uint32_t>(expect_tiles.indices.size());
  for (uint32_t n : stub->index_counts) {
    if (n == n2 && n != 3) {
      saw_2d = true;
    }
    if (n == n3) {
      saw_3d = true;
    }
  }
  expect(saw_2d, "2d draw matches tessellated OGRGeometry");
  expect(saw_3d, "3d draw matches OGR TIN");
  expect(n2 != 3, "2d is not a placeholder triangle");
  expect(count_index(stub->index_counts, n_arc) >= 1, "arc drawn");
  expect(count_index(stub->index_counts, n_tiles) >= 1, "tiles drawn");
  expect(count_index(stub->index_counts, n_fan) >= 1, "fan drawn");
  expect(count_index(stub->index_counts, n_tin) >= 1, "tin drawn");
  expect(count_index(stub->index_counts, 3) >= 2, "fan + tin triangles");
  expect(count_index(stub->index_counts, n_grid) >= 1, "grid drawn");
  expect(count_index(stub->index_counts, n_ras) >= 1, "raster drawn");

#ifdef HAS_FLYCUBE
  // Identity-only by default. FlyCube init/execute can hang headless;
  // set RUN_FLYCUBE_GPU=1 to exercise the real path (same as rhi_test).
  const char* run_gpu = base::switch_cstr("run-flycube-gpu");
  const bool want_gpu = run_gpu && run_gpu[0] == '1' && run_gpu[1] == '\0';
  if (!want_gpu) {
    std::fprintf(stdout,
                 "unified_draw_test: skip FlyCube init "
                 "(set RUN_FLYCUBE_GPU=1)\n");
  } else {
    std::unique_ptr<render::rhi::Device> fly(create_device(Backend::kDx12));
    expect(fly != nullptr, "flycube device object");
    if (fly->initialize(DeviceDesc())) {
      render::rhi::CommandList* flist = fly->create_command_list();
      expect(gpu.record(fly.get(), flist, 64, 64), "flycube record");
      expect(fly->execute(flist), "flycube execute");
      fly->present();
      // Leak flist / skip gpu.release under FlyCube CRT delete hangs.
    }
    fly->shutdown();
  }
#endif

  device->shutdown();

  if (g_fails) {
    std::fprintf(stderr, "unified_draw_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "unified_draw_test: ok\n");
  std::fflush(stdout);
  // Same FlyCube-linked CRT hang as scene_gpu_test after NullDevice stub leaks.
  std::_Exit(0);
}
