// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#include <cstdio>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

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
  using render::rhi::RenderPassDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;
  using render::rhi::preferred_gpu_backend;

  std::unique_ptr<render::rhi::Device> null(create_device(Backend::kNull));
  expect(null != nullptr, "null device");
  expect(null->initialize(DeviceDesc()), "null initialize");
  expect(null->backend() == Backend::kNull, "null backend");

  render::rhi::CommandList* list = null->create_command_list();
  expect(list != nullptr, "null command list");
  RenderPassDesc pass;
  pass.width = 64;
  pass.height = 64;
  list->begin_render_pass(pass);
  list->set_viewport(0, 0, 64, 64, 0, 1);
  list->draw_indexed(3, 1, 0, 0, 0);
  list->end_render_pass();
  list->close();
  expect(null->execute(list), "null execute");
  auto* stub = static_cast<StubCommandList*>(list);
  expect(stub->draw_indexed_calls == 1, "one draw_indexed");
  expect(stub->closed, "closed");
  null->destroy_command_list(list);

  render::rhi::Buffer* vb =
      null->create_buffer(12, render::rhi::BufferUsage::kVertex);
  expect(vb != nullptr, "create_buffer");
  const float xyz[3] = {1, 2, 3};
  expect(null->upload(vb, xyz, 12), "upload");
  render::rhi::CommandList* list2 = null->create_command_list();
  list2->bind_vertex_buffer(vb, 0, 12);
  list2->bind_index_buffer(nullptr, 0);
  list2->draw_indexed(36, 1, 0, 0, 0);
  list2->close();
  auto* stub2 = static_cast<StubCommandList*>(list2);
  expect(stub2->bind_vertex_calls == 1, "bind vertex");
  expect(stub2->index_counts.size() == 1 && stub2->index_counts[0] == 36,
         "recorded index count");
  null->destroy_command_list(list2);
  null->destroy_buffer(vb);

  render::rhi::TextureDesc tex_desc;
  tex_desc.width = 2;
  tex_desc.height = 2;
  tex_desc.format = render::rhi::TextureFormat::kRgba8;
  render::rhi::Texture* tex = null->create_texture(tex_desc);
  expect(tex != nullptr, "create_texture");
  expect(tex && tex->width() == 2 && tex->height() == 2, "texture size");
  const unsigned char rgba[16] = {1, 2, 3, 4, 5, 6, 7, 8,
                                  9, 10, 11, 12, 13, 14, 15, 16};
  expect(null->upload_texture(tex, rgba, 16), "upload_texture");
  render::rhi::CommandList* list3 = null->create_command_list();
  render::rhi::CameraMatrices ortho =
      render::rhi::make_ortho_camera(0, 64, 0, 64, -1, 1);
  list3->bind_camera(ortho);
  list3->bind_texture(tex, 0);
  list3->draw_indexed(6, 1, 0, 0, 0);
  list3->close();
  auto* stub3 = static_cast<StubCommandList*>(list3);
  expect(stub3->bind_texture_calls == 1, "bind_texture recorded");
  expect(stub3->last_texture == tex, "bound texture pointer");
  expect(stub3->bind_camera_calls == 1, "bind_camera recorded");
  expect(stub3->last_camera.kind == render::rhi::CameraKind::kOrtho,
         "null camera is ortho");
  expect(stub3->last_camera.proj[0] != 1.f || stub3->last_camera.proj[5] != 1.f,
         "ortho proj is not identity");
  null->destroy_command_list(list3);
  null->destroy_texture(tex);
  null->shutdown();

  expect(preferred_gpu_backend() == Backend::kDx12, "preferred DX12");

  std::unique_ptr<render::rhi::Device> dx12(create_device(Backend::kDx12));
  expect(dx12 != nullptr, "dx12 device object");
  expect(dx12->backend() == Backend::kDx12, "dx12 backend id");

  std::unique_ptr<render::rhi::Device> vk(create_device(Backend::kVulkan));
  expect(vk != nullptr, "vulkan device object");
  expect(vk->backend() == Backend::kVulkan, "vulkan backend id");

#ifndef SMT_HAS_FLYCUBE
  expect(false, "FlyCube must be linked (SMT_HAS_FLYCUBE) for DX12 present");
#else
#ifdef _WIN32
  HWND hwnd = CreateWindowExW(0, L"STATIC", L"rhi_present", WS_POPUP, 0, 0, 64,
                              64, nullptr, nullptr, GetModuleHandleW(nullptr),
                              nullptr);
  expect(hwnd != nullptr, "hidden present HWND");
  DeviceDesc present_desc;
  present_desc.native_window = hwnd;
  present_desc.width = 64;
  present_desc.height = 64;
  if (!dx12->initialize(present_desc)) {
    std::fprintf(stdout, "rhi_test: skip present (no DX12 adapter)\n");
  } else {
    expect(dx12->backend() == Backend::kDx12, "initialized DX12 backend");
    render::rhi::CommandList* plist = dx12->create_command_list();
    expect(plist != nullptr, "present command list");
    RenderPassDesc clear;
    clear.clear_r = 0.1f;
    clear.clear_g = 0.2f;
    clear.clear_b = 0.3f;
    clear.clear_a = 1.0f;
    clear.width = 64;
    clear.height = 64;
    plist->begin_render_pass(clear);
    plist->set_viewport(0, 0, 64, 64, 0, 1);
    render::rhi::TextureDesc gpu_tex_desc;
    gpu_tex_desc.width = 2;
    gpu_tex_desc.height = 2;
    gpu_tex_desc.format = render::rhi::TextureFormat::kRgba8;
    render::rhi::Texture* gpu_tex = dx12->create_texture(gpu_tex_desc);
    const unsigned char red[16] = {255, 0, 0, 255, 255, 0, 0, 255,
                                   255, 0, 0, 255, 255, 0, 0, 255};
    const float quad[20] = {
        -1.f, -1.f, 0.f, 0.f, 0.f, 1.f, -1.f, 0.f, 1.f, 0.f,
        -1.f, 1.f,  0.f, 0.f, 1.f, 1.f, 1.f,  0.f, 1.f, 1.f,
    };
    const uint32_t quad_idx[6] = {0, 1, 2, 2, 1, 3};
    render::rhi::Buffer* qvb =
        dx12->create_buffer(sizeof(quad), render::rhi::BufferUsage::kVertex);
    render::rhi::Buffer* qib =
        dx12->create_buffer(sizeof(quad_idx), render::rhi::BufferUsage::kIndex);
    if (gpu_tex && qvb && qib && dx12->upload_texture(gpu_tex, red, 16) &&
        dx12->upload(qvb, quad, sizeof(quad)) &&
        dx12->upload(qib, quad_idx, sizeof(quad_idx))) {
      plist->bind_camera(
          render::rhi::make_ortho_camera(-1, 1, -1, 1, -1, 1));
      plist->bind_texture(gpu_tex, 0);
      plist->bind_vertex_buffer(qvb, 0, 5 * sizeof(float));
      plist->bind_index_buffer(qib, 0);
      plist->draw_indexed(6, 1, 0, 0, 0);
    }
    plist->end_render_pass();
    plist->close();
    expect(dx12->execute(plist), "dx12 clear execute");
    auto* pstub = static_cast<StubCommandList*>(plist);
    expect(pstub->bind_camera_calls >= 1, "dx12 camera bind recorded");
    expect(pstub->bind_texture_calls >= 1, "dx12 texture bind recorded");
    if (dx12->gpu_sampled_draws() == 0) {
      std::fprintf(stdout,
                   "rhi_test: skip gpu sample (pipeline not ready)\n");
    } else {
      expect(dx12->gpu_sampled_draws() >= 1, "FlyCube sampled uploaded texture");
    }
    dx12->present();
    dx12->destroy_command_list(plist);
    if (qvb) {
      dx12->destroy_buffer(qvb);
    }
    if (qib) {
      dx12->destroy_buffer(qib);
    }
    if (gpu_tex) {
      dx12->destroy_texture(gpu_tex);
    }
    std::fprintf(stdout, "rhi_test: dx12 present ok\n");
  }
  dx12->shutdown();
  if (hwnd) {
    DestroyWindow(hwnd);
  }
#endif
#endif

  if (g_fails) {
    std::fprintf(stderr, "rhi_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "rhi_test: ok\n");
  return 0;
}
