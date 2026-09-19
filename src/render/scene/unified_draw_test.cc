// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"
#include "render/scene/scene.h"
#include "gis/world/scene.h"
#include "gis/world/tessellate.h"

#include "algorithm/geo/geometry.h"
#include "algorithm/geo/geometry.h"
#include "gis/layer/layer.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

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

class TestRasterLayer : public gis::SmtRasterLayer {
 public:
  bool Create() override { return true; }
  bool Open(const char*) override { return true; }
  bool Close() override { return true; }
  bool Fetch(gis::eSmtFetchType) override { return true; }
  long CreaterRaster(const char* buf, long size, const base::fRect& rect,
                     long code) override {
    buf_ = buf;
    size_ = size;
    rect_ = rect;
    SetLayerRect(rect);
    (void)code;
    return SMT_ERR_NONE;
  }
  long SetRasterRect(const base::fRect& rect) override {
    rect_ = rect;
    return SMT_ERR_NONE;
  }
  long GetRaster(char*& buf, long& size, base::fRect& rect,
                 long& code) const override {
    return GetRasterNoClone(buf, size, rect, code);
  }
  long GetRasterNoClone(char*& buf, long& size, base::fRect& rect,
                        long& code) const override {
    buf = const_cast<char*>(buf_);
    size = size_;
    rect = rect_;
    code = 0;
    return buf_ && size_ > 0 ? SMT_ERR_NONE : SMT_ERR_UNSUPPORTED;
  }
  long GetRasterRect(base::fRect& rect) const override {
    rect = rect_;
    return SMT_ERR_NONE;
  }

 private:
  const char* buf_ = nullptr;
  long size_ = 0;
  base::fRect rect_{};
};

class TestTileLayer : public gis::SmtTileLayer {
 public:
  bool Create() override { return true; }
  bool Open(const char*) override { return true; }
  bool Close() override { return true; }
  bool Fetch(gis::eSmtFetchType) override { return true; }
  void CalEnvelope() override {}
  int GetTileCount() const override { return static_cast<int>(tiles_.size()); }
  void MoveFirst() const override { it_ = 0; }
  void MoveNext() const override { ++it_; }
  void MoveLast() const override {
    it_ = tiles_.empty() ? 0 : tiles_.size() - 1;
  }
  void Delete() override {}
  bool IsEnd() const override { return it_ >= tiles_.size(); }
  void DeleteAll() override { tiles_.clear(); }
  long AppendTile(const base::SmtTile* tile, bool) override {
    if (tile) {
      tiles_.push_back(*tile);
    }
    return SMT_ERR_NONE;
  }
  long UpdateTile(const base::SmtTile*) override { return SMT_ERR_NONE; }
  long DeleteTile(const base::SmtTile*) override { return SMT_ERR_NONE; }
  base::SmtTile* GetTile() const override {
    return it_ < tiles_.size() ? const_cast<base::SmtTile*>(&tiles_[it_])
                               : nullptr;
  }
  base::SmtTile* GetTile(int index) const override {
    if (index < 0 || index >= static_cast<int>(tiles_.size())) {
      return nullptr;
    }
    return const_cast<base::SmtTile*>(&tiles_[static_cast<size_t>(index)]);
  }
  base::SmtTile* GetTileByID(uint) const override { return nullptr; }

 private:
  std::vector<base::SmtTile> tiles_;
  mutable size_t it_ = 0;
};

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
  gis::TessMesh expect_2d;
  expect(gis::tessellate_geoms(geoms, 3, expect_2d),
         "tessellate point+line+poly");
  expect(expect_2d.indices.size() >= 12, "2d feature index count");

  geo::Smt3DSurface surf;
  OGRPoint p0(0, 0, 0);
  OGRPoint p1(1, 0, 0);
  OGRPoint p2(1, 1, 0);
  OGRPoint p3(0, 1, 0);
  surf.add_point(&p0);
  surf.add_point(&p1);
  surf.add_point(&p2);
  surf.add_point(&p3);
  base::Smt3DTriangle t0;
  t0.a = 0;
  t0.b = 1;
  t0.c = 2;
  base::Smt3DTriangle t1;
  t1.a = 0;
  t1.b = 2;
  t1.c = 3;
  surf.add_triangle(&t0);
  surf.add_triangle(&t1);
  gis::TessMesh expect_3d;
  expect(gis::tessellate_3d_surface(&surf, expect_3d), "tess 3d surf");
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

  gis::World world;
  expect(world.attach_vector_geoms("roads", geoms, 3) != nullptr,
         "attach 2d GIS geoms");
  expect(world.attach_3d_geometry(&poly3d, "surface") != nullptr,
         "attach 3d GIS surface");
  expect(world.node_count() == 2, "vector + 3d");
  expect(world.node_at(0)->kind == gis::NodeKind::kVectorLayer,
         "vector kind");
  expect(world.node_at(1)->geom_3d == &poly3d, "3d pointer");

  OGRLineString arc;
  arc.addPoint(0, 0);
  arc.addPoint(1, 1);
  arc.addPoint(2, 0);
  gis::TessMesh expect_arc;
  expect(gis::tessellate_arc(&arc, expect_arc), "tess arc");
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
  gis::TessMesh expect_fan;
  expect(gis::tessellate_fan(&fan, expect_fan), "tess fan");
  expect(expect_fan.indices.size() == 3, "fan index count");
  const OGRGeometry* fan_geoms[] = {&fan};
  expect(world.attach_vector_geoms("fan", fan_geoms, 1) != nullptr,
         "attach fan");

  geo::Tin tin;
  OGRPoint ta(0, 0);
  OGRPoint tb(2, 0);
  OGRPoint tc(1, 2);
  tin.add_point(&ta);
  tin.add_point(&tb);
  tin.add_point(&tc);
  base::SmtTriangle tin_tri;
  tin_tri.a = 0;
  tin_tri.b = 1;
  tin_tri.c = 2;
  tin.add_triangle(&tin_tri);
  gis::TessMesh expect_tin;
  expect(gis::tessellate_tin(&tin, expect_tin), "tess tin");
  expect(expect_tin.indices.size() == 3, "tin index count");
  expect(world.attach_tin(&tin, "tin") != nullptr, "attach tin");

  geo::Grid grid(2, 2);
  grid.set_node(0, 0, geo::RawPoint(0, 0));
  grid.set_node(0, 1, geo::RawPoint(1, 0));
  grid.set_node(1, 0, geo::RawPoint(0, 1));
  grid.set_node(1, 1, geo::RawPoint(1, 1));
  gis::TessMesh expect_grid;
  expect(gis::tessellate_grid(&grid, expect_grid), "tess grid");
  expect(expect_grid.indices.size() == 6, "grid index count");
  expect(world.attach_grid(&grid, "grid") != nullptr, "attach grid");

  TestRasterLayer raster;
  raster.SetLayerName("ras");
  base::fRect ras_rect;
  ras_rect.lb.x = 0;
  ras_rect.lb.y = 0;
  ras_rect.rt.x = 8;
  ras_rect.rt.y = 6;
  char pixels[4] = {9, 8, 7, 6};
  raster.CreaterRaster(pixels, 4, ras_rect, 1);
  gis::TessMesh expect_ras;
  expect(gis::tessellate_raster_layer(&raster, expect_ras),
         "tess raster");
  expect(expect_ras.indices.size() == 6, "raster index count");
  expect(world.attach_raster_layer(&raster) != nullptr, "attach raster");

  TestTileLayer tiles;
  tiles.SetLayerName("tiles");
  base::SmtTile tile0;
  tile0.lID = 1;
  tile0.rtTileRect.lb.x = 0;
  tile0.rtTileRect.lb.y = 0;
  tile0.rtTileRect.rt.x = 2;
  tile0.rtTileRect.rt.y = 2;
  base::SmtTile tile1;
  tile1.lID = 2;
  tile1.rtTileRect.lb.x = 2;
  tile1.rtTileRect.lb.y = 0;
  tile1.rtTileRect.rt.x = 4;
  tile1.rtTileRect.rt.y = 2;
  tiles.AppendTile(&tile0, false);
  tiles.AppendTile(&tile1, false);
  gis::TessMesh expect_tiles;
  expect(gis::tessellate_tile_layer(&tiles, expect_tiles), "tess tiles");
  expect(expect_tiles.indices.size() == 12, "tile index count");
  expect(world.attach_tile_layer(&tiles) != nullptr, "attach tiles");
  expect(world.node_count() == 8, "mixed remaining GIS nodes");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device && device->initialize(DeviceDesc()), "null initialize");

  render::scene::GpuScene gpu;
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
  expect(stub->bind_texture_calls >= 1, "raster texture bind");
  expect(stub->last_texture != nullptr, "raster texture pointer");
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
  expect(saw_3d, "3d draw matches Smt3DSurface");
  expect(n2 != 3, "2d is not a placeholder triangle");
  expect(count_index(stub->index_counts, n_arc) >= 1, "arc drawn");
  expect(count_index(stub->index_counts, n_tiles) >= 1, "tiles drawn");
  expect(count_index(stub->index_counts, 12) >= 2, "arc + two tile quads");
  expect(count_index(stub->index_counts, n_fan) >= 1, "fan drawn");
  expect(count_index(stub->index_counts, n_tin) >= 1, "tin drawn");
  expect(count_index(stub->index_counts, 3) >= 2, "fan + tin triangles");
  expect(count_index(stub->index_counts, n_grid) >= 1, "grid drawn");
  expect(count_index(stub->index_counts, n_ras) >= 1, "raster drawn");
  expect(count_index(stub->index_counts, 6) >= 3, "grid + raster + 3d quads");

#ifdef SMT_HAS_FLYCUBE
  // Identity-only by default. FlyCube init/execute can hang headless;
  // set SMT_RUN_FLYCUBE_GPU=1 to exercise the real path (same as rhi_test).
  const char* run_gpu = std::getenv("SMT_RUN_FLYCUBE_GPU");
  const bool want_gpu = run_gpu && run_gpu[0] == '1' && run_gpu[1] == '\0';
  if (!want_gpu) {
    std::fprintf(stdout,
                 "unified_draw_test: skip FlyCube init "
                 "(set SMT_RUN_FLYCUBE_GPU=1)\n");
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
