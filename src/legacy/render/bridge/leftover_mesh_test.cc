// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/bridge/leftover_mesh.h"

#include <cstdio>
#include <memory>

#include "legacy/render/render3d/3drenderdefs.h"
#include "legacy/render/render3d/indexbuffer.h"
#include "legacy/render/render3d/vertexbuffer.h"
#include "render/rhi/rhi.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// TessMesh-shaped 2D GIS output (xyz triples + triangle indices) without
// calling gis::tessellate_* (sibling owns that path).
void fill_tess_shaped_2d(float* xyz, uint32_t* indices, uint32_t* xyz_floats,
                         uint32_t* index_count) {
  // Point sprite (3 verts) + line quad (4 verts) + triangle poly (3 verts).
  const float src[] = {
      1.f,   2.05f, 0.f, 0.95f,  1.95f, 0.f,  1.05f, 1.95f, 0.f,  0.f,
      0.05f, 0.f,   0.f, -0.05f, 0.f,   10.f, 0.05f, 0.f,   10.f, -0.05f,
      0.f,   0.f,   0.f, 0.f,    2.f,   0.f,  0.f,   1.f,   2.f,  0.f,
  };
  const uint32_t idx[] = {0, 1, 2, 3, 4, 5, 4, 6, 5, 7, 8, 9};
  *xyz_floats = static_cast<uint32_t>(sizeof(src) / sizeof(src[0]));
  *index_count = static_cast<uint32_t>(sizeof(idx) / sizeof(idx[0]));
  for (uint32_t i = 0; i < *xyz_floats; ++i) {
    xyz[i] = src[i];
  }
  for (uint32_t i = 0; i < *index_count; ++i) {
    indices[i] = idx[i];
  }
}

}  // namespace

int main() {
  using render::rhi::Backend;
  using render::rhi::create_device;
  using render::rhi::DeviceDesc;
  using render::rhi::RenderPassDesc;
  using render::rhi::StubCommandList;
  using render::scene::create_host_index_buffer;
  using render::scene::create_host_vertex_buffer;
  using render::scene::destroy_host_index_buffer;
  using render::scene::destroy_host_vertex_buffer;
  using render::scene::destroy_leftover_mesh;
  using render::scene::LeftoverGpuMesh;
  using render::scene::record_leftover_draw;
  using render::scene::upload_leftover_buffers;
  using render::scene::upload_xyz_mesh;

  expect(!upload_leftover_buffers(nullptr, nullptr, nullptr, nullptr),
         "null upload rejected");
  expect(!record_leftover_draw(nullptr, LeftoverGpuMesh()),
         "null record rejected");

  render::SmtVertexBuffer* vb = create_host_vertex_buffer(3, render::VF_XYZ);
  render::SmtIndexBuffer* ib = create_host_index_buffer(3);
  expect(vb != nullptr, "host leftover VB");
  expect(ib != nullptr, "host leftover IB");
  if (vb && ib) {
    expect(vb->Lock() == SMT_ERR_NONE, "lock VB");
    vb->Vertex(0.f, 0.f, 0.f);
    vb->Vertex(1.f, 0.f, 0.f);
    vb->Vertex(0.f, 1.f, 0.f);
    expect(vb->Unlock() == SMT_ERR_NONE, "unlock VB");
    expect(vb->GetVertexCount() == 3, "leftover vertex count");
    expect(vb->GetVertexStride() == 3 * sizeof(float), "leftover xyz stride");
    expect(vb->GetVertexFormat() == render::VF_XYZ, "leftover VF_XYZ");

    expect(ib->Lock() == SMT_ERR_NONE, "lock IB");
    ib->Index(0);
    ib->Index(1);
    ib->Index(2);
    expect(ib->Unlock() == SMT_ERR_NONE, "unlock IB");
    expect(ib->GetIndexCount() == 3, "leftover index count");
  }

  float xyz2d[32];
  uint32_t idx2d[16];
  uint32_t nxyz = 0;
  uint32_t nidx = 0;
  fill_tess_shaped_2d(xyz2d, idx2d, &nxyz, &nidx);
  expect(nidx >= 12, "2d tess-shaped index count");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device && device->initialize(DeviceDesc()), "null initialize");
  expect(device->backend() == Backend::kNull, "same null Device as 2D GIS");

  LeftoverGpuMesh mesh_2d;
  expect(upload_xyz_mesh(device.get(), xyz2d, nxyz, idx2d, nidx, &mesh_2d),
         "upload tess-shaped 2d");

  LeftoverGpuMesh mesh_3d;
  expect(upload_leftover_buffers(device.get(), vb, ib, &mesh_3d),
         "upload leftover VB/IB");
  expect(mesh_3d.index_count == 3, "leftover gpu index count");
  expect(mesh_3d.stride == 3 * sizeof(float), "leftover gpu stride");

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "shared command list");
  RenderPassDesc pass;
  pass.clear_r = 0;
  pass.clear_g = 0.2f;
  pass.clear_b = 0.4f;
  pass.clear_a = 1;
  pass.width = 64;
  pass.height = 64;
  list->begin_render_pass(pass);
  list->set_viewport(0, 0, 64, 64, 0, 1);
  expect(record_leftover_draw(list, mesh_2d), "record 2d tessellation");
  expect(record_leftover_draw(list, mesh_3d), "record leftover 3d");
  list->end_render_pass();
  list->close();
  expect(device->execute(list), "null execute");
  device->present();

  auto* stub = static_cast<StubCommandList*>(list);
  expect(stub->closed, "list closed");
  expect(stub->draw_indexed_calls == 2, "2d and leftover on one list");
  expect(stub->bind_vertex_calls == 2, "two vertex binds");
  expect(stub->bind_index_calls == 2, "two index binds");
  expect(stub->index_counts.size() == 2, "two recorded draws");
  if (stub->index_counts.size() == 2) {
    expect(stub->index_counts[0] == nidx, "2d draw matches tess-shaped mesh");
    expect(stub->index_counts[1] == 3, "leftover draw matches VB/IB");
    expect(stub->index_counts[0] != stub->index_counts[1],
           "2d and leftover counts differ");
  }

  destroy_leftover_mesh(device.get(), &mesh_2d);
  destroy_leftover_mesh(device.get(), &mesh_3d);
  device->destroy_command_list(list);
  device->shutdown();
  destroy_host_index_buffer(ib);
  destroy_host_vertex_buffer(vb);

  if (g_fails) {
    std::fprintf(stderr, "leftover_mesh_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "leftover_mesh_test: ok\n");
  return 0;
}
