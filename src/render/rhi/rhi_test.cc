// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#include <cstdio>
#include <cstdlib>
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

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  std::unique_ptr<render::rhi::Device> null(create_device(Backend::kNull));
  expect(null != nullptr, "null device");
  expect(null->initialize(DeviceDesc()), "null initialize");
  expect(null->backend() == Backend::kNull, "null backend");

  // One CommandList covers draw / buffer / texture / camera / solid recording.
  // Multiple create/destroy cycles have hung under FlyCube-linked headless
  // hosts (operator delete / allocator), so keep a single list and leak it.
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

  render::rhi::Buffer* vb =
      null->create_buffer(12, render::rhi::BufferUsage::kVertex);
  expect(vb != nullptr, "create_buffer");
  const float xyz[3] = {1, 2, 3};
  expect(null->upload(vb, xyz, 12), "upload");
  list->bind_vertex_buffer(vb, 0, 12);
  list->bind_index_buffer(nullptr, 0);
  list->draw_indexed(36, 1, 0, 0, 0);
  expect(stub->bind_vertex_calls == 1, "bind vertex");
  expect(stub->index_counts.size() >= 2 && stub->index_counts.back() == 36,
         "recorded index count");

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
  render::rhi::CameraMatrices ortho =
      render::rhi::make_ortho_camera(0, 64, 0, 64, -1, 1);
  list->bind_camera(ortho);
  list->bind_texture(tex, 0);
  list->draw_indexed(6, 1, 0, 0, 0);
  expect(stub->bind_texture_calls == 1, "bind_texture recorded");
  expect(stub->last_texture == tex, "bound texture pointer");
  expect(stub->bind_camera_calls == 1, "bind_camera recorded");
  expect(stub->last_camera.kind == render::rhi::CameraKind::kOrtho,
         "null camera is ortho");
  expect(stub->last_camera.proj[0] != 1.f || stub->last_camera.proj[5] != 1.f,
         "ortho proj is not identity");
  list->set_solid_color(0.1f, 0.2f, 0.3f, 0.4f);
  expect(stub->set_solid_color_calls == 1, "set_solid_color recorded");
  expect(stub->solid_r == 0.1f && stub->solid_g == 0.2f &&
             stub->solid_b == 0.3f && stub->solid_a == 0.4f,
         "solid rgba values");
  null->shutdown();

  expect(preferred_gpu_backend() == Backend::kDx12, "preferred DX12");

#ifndef SMT_HAS_FLYCUBE
  expect(false, "FlyCube must be linked (SMT_HAS_FLYCUBE) for DX12 present");
#else
  // Identity-only by default. HWND init/execute/present can hang headless;
  // set SMT_RUN_FLYCUBE_GPU=1 to exercise the real path.
  const char* run_gpu = std::getenv("SMT_RUN_FLYCUBE_GPU");
  const bool want_gpu = run_gpu && run_gpu[0] == '1' && run_gpu[1] == '\0';

  std::unique_ptr<render::rhi::Device> dx12(create_device(Backend::kDx12));
  expect(dx12 != nullptr, "dx12 device object");
  expect(dx12->backend() == Backend::kDx12, "dx12 backend id");

  std::unique_ptr<render::rhi::Device> vk(create_device(Backend::kVulkan));
  expect(vk != nullptr, "vulkan device object");
  expect(vk->backend() == Backend::kVulkan, "vulkan backend id");

#ifdef _WIN32
  if (!want_gpu) {
    std::fprintf(stdout,
                 "rhi_test: skip FlyCube HWND init "
                 "(set SMT_RUN_FLYCUBE_GPU=1)\n");
  } else {
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"rhi_present", WS_POPUP, 0, 0,
                                64, 64, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
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
      plist->set_solid_color(0.2f, 0.4f, 0.6f, 1.f);
      plist->end_render_pass();
      plist->close();
      expect(dx12->execute(plist), "dx12 clear execute");
      dx12->present();
      std::fprintf(stdout, "rhi_test: dx12 present ok\n");
    }
    dx12->shutdown();
    if (hwnd) {
      DestroyWindow(hwnd);
    }
  }
#endif
  dx12.reset();
  vk.reset();
#endif

  if (g_fails) {
    std::fprintf(stderr, "rhi_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "rhi_test: ok\n");
  return 0;
}
