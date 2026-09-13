// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy_render/bridge/leftover_record.h"

#include "algorithm/geo/geometry.h"
#include "sdb/layer/layer.h"
#include "render/rhi/rhi.h"
#include "sdb/scene/tessellate.h"

#include "legacy_render/render3d/3drenderdefs.h"
#include "legacy_render/render3d/indexbuffer.h"
#include "legacy_render/render3d/vertexbuffer.h"

#include <cstdio>
#include <memory>

// Null-device path only. Do not create FlyCube here — use SMT_RUN_FLYCUBE_GPU=1
// via rhi_test for real DX12. Per-layer MapLayer brush on GpuScene remains
// TODO in leftover_record::record_map (avoid style DLL in sdb/scene).

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

class TestRasterLayer : public sdb::SmtRasterLayer {
 public:
  bool Create() override { return true; }
  bool Open(const char*) override { return true; }
  bool Close() override { return true; }
  bool Fetch(sdb::eSmtFetchType) override { return true; }
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

}  // namespace

int main() {
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;
  using render::scene::LeftoverRecorder;
  using render::scene::create_host_index_buffer;
  using render::scene::create_host_vertex_buffer;
  using render::scene::destroy_host_index_buffer;
  using render::scene::destroy_host_vertex_buffer;

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
  sdb::scene::TessMesh expect_2d;
  expect(sdb::scene::tessellate_geoms(geoms, 3, expect_2d), "tess 2d");
  expect(expect_2d.indices.size() >= 12, "2d not placeholder");

  sdb::scene::World world;
  expect(world.attach_vector_geoms("roads", geoms, 3) != nullptr, "attach 2d");

  TestRasterLayer raster;
  raster.SetLayerName("ras");
  base::fRect ras_rect;
  ras_rect.lb.x = 0;
  ras_rect.lb.y = 0;
  ras_rect.rt.x = 8;
  ras_rect.rt.y = 6;
  char pixels[4] = {9, 8, 7, 6};
  raster.CreaterRaster(pixels, 4, ras_rect, 1);
  expect(world.attach_raster_layer(&raster) != nullptr, "attach raster");

  TestRasterLayer empty_ras;
  empty_ras.SetLayerName("empty");
  empty_ras.SetRasterRect(ras_rect);
  sdb::scene::World empty_world;
  expect(empty_world.attach_raster_layer(&empty_ras) != nullptr,
         "attach empty raster");

  render::SmtVertexBuffer* vb =
      create_host_vertex_buffer(3, render::VF_XYZ);
  render::SmtIndexBuffer* ib = create_host_index_buffer(3);
  expect(vb && ib, "host leftover VB/IB");
  if (vb && ib) {
    expect(vb->Lock() == SMT_ERR_NONE, "lock VB");
    vb->Vertex(0.f, 0.f, 0.f);
    vb->Vertex(1.f, 0.f, 0.f);
    vb->Vertex(0.f, 1.f, 0.f);
    expect(vb->Unlock() == SMT_ERR_NONE, "unlock VB");
    expect(ib->Lock() == SMT_ERR_NONE, "lock IB");
    ib->Index(0);
    ib->Index(1);
    ib->Index(2);
    expect(ib->Unlock() == SMT_ERR_NONE, "unlock IB");
  }

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device && device->initialize(DeviceDesc()), "null initialize");

  LeftoverRecorder rec;
  expect(rec.attach(device.get()), "attach shared Device");
  expect(rec.begin(64, 64), "begin leftover list");
  expect(rec.record_world(world), "record leftover 2D GIS");
  expect(rec.record_3d(vb, ib), "record leftover 3D VB/IB");
  expect(rec.finish(), "finish leftover list");

  auto* stub = static_cast<StubCommandList*>(rec.list());
  expect(stub != nullptr, "shared list");
  expect(stub && stub->closed, "list closed");
  expect(stub && stub->draw_indexed_calls >= 3, "2d + raster + leftover 3d");
  expect(stub && stub->bind_texture_calls >= 1, "raster texture bind");
  expect(stub && stub->last_texture != nullptr, "texture bound");
  expect(stub && stub->bind_camera_calls >= 2,
         "ortho GIS + perspective leftover 3D");
  expect(stub && stub->last_camera.kind == render::rhi::CameraKind::kPerspective,
         "last leftover bind is perspective");

  bool saw_2d = false;
  bool saw_3d = false;
  bool saw_ras = false;
  const uint32_t n2 = static_cast<uint32_t>(expect_2d.indices.size());
  if (stub) {
    for (uint32_t n : stub->index_counts) {
      if (n == n2 && n != 3) {
        saw_2d = true;
      }
      if (n == 3) {
        saw_3d = true;
      }
      if (n == 6) {
        saw_ras = true;
      }
    }
  }
  expect(saw_2d, "2d leftover GIS matches tessellation");
  expect(saw_3d, "leftover 3d draw_indexed on same list");
  expect(saw_ras, "raster envelope quad");
  expect(n2 != 3, "2d is not placeholder triangle");

  LeftoverRecorder solid;
  expect(solid.attach(device.get()), "attach solid device");
  expect(solid.begin(64, 64), "begin solid");
  expect(solid.record_world(empty_world), "record solid raster");
  expect(solid.finish(), "finish solid");
  auto* solid_stub = static_cast<StubCommandList*>(solid.list());
  expect(solid_stub && solid_stub->draw_indexed_calls >= 1, "solid quad drawn");
  expect(solid_stub && solid_stub->bind_texture_calls == 0,
         "empty raster has no texture bind");
  expect(solid_stub && solid_stub->set_solid_color_calls >= 1,
         "solid color applied to untextured mesh");

  // ensure_device without HWND must stay on Null (no FlyCube create/init).
  LeftoverRecorder preferred;
  preferred.set_native_window(nullptr);
  expect(preferred.begin(32, 32), "begin null path without HWND");
  expect(preferred.device() != nullptr, "null device without HWND");
  expect(preferred.device()->backend() == Backend::kNull,
         "no HWND keeps null backend");
  // Skip preferred.release() / rec.release() churn: NullDevice destroy_* already
  // leaks stubs; destructors still detach safely.

  device->shutdown();
  destroy_host_index_buffer(ib);
  destroy_host_vertex_buffer(vb);

  if (g_fails) {
    std::fprintf(stderr, "leftover_record_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "leftover_record_test: ok\n");
  return 0;
}
