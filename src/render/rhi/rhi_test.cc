// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

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

std::string join_file(const std::string& dir, const char* name) {
  if (dir.empty()) {
    return name;
  }
  const char last = dir.back();
  if (last == '/' || last == '\\') {
    return dir + name;
  }
  return dir + "/" + name;
}

std::string sibling_path(const char* name) {
  std::string file(__FILE__);
  const size_t slash = file.find_last_of("/\\");
  const std::string dir =
      (slash == std::string::npos) ? std::string() : file.substr(0, slash + 1);
  const std::string candidates[] = {
      join_file(dir, name),
      join_file(std::string("src/render/rhi"), name),
      join_file(std::string("../src/render/rhi"), name),
  };
  for (const std::string& path : candidates) {
    std::ifstream in(path.c_str());
    if (in) {
      return path;
    }
  }
  return candidates[0];
}

bool include_line_mentions_flycube(const std::string& line) {
  if (line.find("#include") == std::string::npos) {
    return false;
  }
  return line.find("FlyCube") != std::string::npos ||
         line.find("flycube") != std::string::npos ||
         line.find("ApiType/ApiType.h") != std::string::npos ||
         line.find("Instance/Instance.h") != std::string::npos ||
         line.find("CommandQueue/CommandQueue.h") != std::string::npos ||
         line.find("Device/Device.h") != std::string::npos;
}

bool public_header_includes_flycube(const char* name) {
  const std::string path = sibling_path(name);
  std::ifstream in(path.c_str());
  if (!in) {
    std::fprintf(stderr, "FAIL: cannot open %s\n", path.c_str());
    return true;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (include_line_mentions_flycube(line)) {
      std::fprintf(stderr, "FAIL: FlyCube include in %s: %s\n", name,
                   line.c_str());
      return true;
    }
  }
  return false;
}

bool wrap_tu_includes_flycube(const char* name) {
  const std::string path = sibling_path(name);
  std::ifstream in(path.c_str());
  if (!in) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (include_line_mentions_flycube(line)) {
      return true;
    }
  }
  return false;
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

  // Stub adapters keep Backend tags; no real GDI/GL raster required.
  std::unique_ptr<render::rhi::Device> gdi(create_device(Backend::kGdi));
  expect(gdi != nullptr && gdi->backend() == Backend::kGdi, "gdi backend id");
  std::unique_ptr<render::rhi::Device> gl(create_device(Backend::kGl));
  expect(gl != nullptr && gl->backend() == Backend::kGl, "gl backend id");

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
  render::rhi::CameraMatrices orbit =
      render::rhi::make_orbit_camera(0.5f, 0.3f, 4.f, 0.785398f, 1.333f, 0.1f,
                                     100.f);
  expect(orbit.kind == render::rhi::CameraKind::kPerspective,
         "orbit camera is perspective");
  expect(std::fabs(orbit.view[12]) > 1e-4f || std::fabs(orbit.view[13]) > 1e-4f ||
             std::fabs(orbit.view[14]) > 1e-4f,
         "orbit view translates eye");
  // look_at is OpenGL-style (camera looks down -Z). Perspective must be RH so
  // a point at the orbit target (origin) lands in front with positive clip w.
  {
    auto mul_col = [](const float* m, float x, float y, float z, float* o) {
      o[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
      o[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
      o[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
      o[3] = m[3] * x + m[7] * y + m[11] * z + m[15];
    };
    auto expect_orbit_visible = [&](float aspect, const char* label) {
      const render::rhi::CameraMatrices cam = render::rhi::make_orbit_camera(
          2.59f, 0.40f, 2.55f, 1.35f, aspect, 0.1f, 100.f);
      float eye[4];
      float clip[4];
      mul_col(cam.view, 0.f, 0.f, 0.f, eye);
      mul_col(cam.proj, eye[0], eye[1], eye[2], clip);
      expect(clip[3] > 0.05f, label);
      expect(std::fabs(clip[0]) <= clip[3] * 1.05f &&
                 std::fabs(clip[1]) <= clip[3] * 1.05f,
             label);
      // Normalized DEM corner must stay on-screen (wide HWND used to fail).
      mul_col(cam.view, 1.5f, 0.1f, -0.9f, eye);
      mul_col(cam.proj, eye[0], eye[1], eye[2], clip);
      expect(clip[3] > 0.05f, label);
      expect(std::fabs(clip[0]) <= clip[3] * 1.25f &&
                 std::fabs(clip[1]) <= clip[3] * 1.25f,
             label);
    };
    expect_orbit_visible(4.f / 3.f, "orbit visible at 4:3");
    expect_orbit_visible(2448.f / 1154.f, "orbit visible at interactive ~2:1");
  }
  const char* k_graphics_vs =
      "float4 main(float3 pos : POSITION) : SV_POSITION { return float4(pos, 1.0); }";
  const char* k_graphics_ps = R"(
cbuffer ColorCB : register(b0)
{
    float4 color;
};
float4 main() : SV_TARGET { return color; }
)";
  const render::rhi::BindingSlot graphics_slots[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kPixel,
       .size_bytes = 16,
       .hlsl_name = "ColorCB"},
  };
  render::rhi::GraphicsPipelineDesc graphics_desc;
  graphics_desc.vertex.hlsl = k_graphics_vs;
  graphics_desc.pixel.hlsl = k_graphics_ps;
  graphics_desc.bindings = graphics_slots;
  graphics_desc.binding_count = 1;
  graphics_desc.camera_slot = -1;
  render::rhi::Pipeline* graphics =
      null->create_graphics_pipeline(graphics_desc);
  expect(graphics != nullptr, "graphics pipeline");
  const float color_rgba[4] = {0.1f, 0.2f, 0.3f, 0.4f};
  list->set_pipeline(graphics);
  list->set_constants(0, color_rgba, sizeof(color_rgba));
  expect(stub->set_pipeline_calls == 1, "set_pipeline recorded");
  expect(stub->last_pipeline == graphics, "graphics pipeline pointer");
  expect(stub->set_constants_calls == 1, "set_constants recorded");
  const render::rhi::StubCommandList::ConstantRecord* color_rec =
      stub->constant_at(0);
  expect(color_rec && color_rec->has_bytes &&
             color_rec->byte_size == sizeof(color_rgba),
         "color constant size");
  expect(color_rec &&
             std::memcmp(color_rec->bytes, color_rgba, sizeof(color_rgba)) == 0,
         "color constant bytes");

  list->set_blend_mode(render::rhi::BlendMode::kSrcAlpha);
  list->set_depth_mode(render::rhi::DepthMode::kTestOnly);
  expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha, "blend srcA");
  expect(stub->last_depth == render::rhi::DepthMode::kTestOnly, "depth test");

  expect(!null->supports_compute(), "null supports_compute false");
  const char* k_compute = R"(
cbuffer ParamsCB : register(b0)
{
    float4 params;
};
[numthreads(1, 1, 1)]
void main() {}
)";
  const render::rhi::BindingSlot compute_slots[] = {
      {.slot = 0,
       .kind = render::rhi::BindingKind::kConstantBuffer,
       .stage = render::rhi::ShaderStage::kCompute,
       .size_bytes = 16,
       .hlsl_name = "ParamsCB"},
  };
  render::rhi::ComputePipelineDesc compute_desc;
  compute_desc.compute.hlsl = k_compute;
  compute_desc.bindings = compute_slots;
  compute_desc.binding_count = 1;
  render::rhi::Pipeline* compute = null->create_compute_pipeline(compute_desc);
  expect(compute != nullptr, "compute pipeline");
  const float compute_words[4] = {32.f, 5.f, 0.9f, 1.f};
  list->set_pipeline(compute);
  list->set_constants(0, compute_words, sizeof(compute_words));
  list->bind_compute_uav(tex, 0);
  list->dispatch(4, 4, 1);
  list->uav_barrier();
  expect(stub->set_pipeline_calls == 2, "compute set_pipeline");
  expect(stub->last_pipeline == compute, "compute pipeline pointer");
  expect(stub->set_constants_calls == 2, "compute constants recorded");
  const render::rhi::StubCommandList::ConstantRecord* compute_rec =
      stub->constant_at(0);
  expect(compute_rec && compute_rec->has_bytes &&
             compute_rec->byte_size == sizeof(compute_words) &&
             std::memcmp(compute_rec->bytes, compute_words,
                         sizeof(compute_words)) == 0,
         "compute constant bytes");
  list->dispatch(4, 4, 1);
  expect(stub->bind_compute_uav_calls == 1, "bind uav");
  expect(stub->dispatch_calls == 2, "dispatch");
  expect(stub->last_dispatch_x == 4 && stub->last_dispatch_y == 4,
         "dispatch groups");
  expect(stub->uav_barrier_calls == 1, "uav barrier");

  RenderPassDesc depth_pass;
  depth_pass.width = 32;
  depth_pass.height = 32;
  depth_pass.enable_depth = true;
  depth_pass.depth_load_op = render::rhi::DepthLoadOp::kClear;
  list->begin_render_pass(depth_pass);
  list->end_render_pass();
  expect(stub->depth_enabled_pass_calls >= 1, "depth-enabled pass");
  expect(stub->end_render_pass_calls >= 1, "end_render_pass counted");
  expect(stub->begin_render_pass_calls >= 2, "multi begin_render_pass");

  // Same-frame 2D + 3D: one Device, one CommandList, one execute/submit.
  const uint32_t submits_before = null->execute_count();
  render::rhi::CommandList* frame = null->create_command_list();
  expect(frame != nullptr, "same-frame command list");
  auto* frame_stub = static_cast<StubCommandList*>(frame);
  RenderPassDesc frame_pass;
  frame_pass.width = 64;
  frame_pass.height = 64;
  frame->begin_render_pass(frame_pass);
  frame->bind_camera(ortho);
  frame->draw_indexed(6, 1, 0, 0, 0);
  expect(frame_stub->last_camera.kind == render::rhi::CameraKind::kOrtho,
         "same-frame 2d ortho");
  frame->bind_camera(orbit);
  frame->draw_indexed(36, 1, 0, 0, 0);
  expect(frame_stub->last_camera.kind == render::rhi::CameraKind::kPerspective,
         "same-frame 3d perspective");
  frame->end_render_pass();
  frame->close();
  expect(frame_stub->draw_indexed_calls == 2, "same-frame two draws");
  expect(frame_stub->bind_camera_calls == 2, "same-frame two cameras");
  expect(null->execute(frame), "same-frame execute");
  expect(null->execute_count() == submits_before + 1,
         "same-frame single submit");

  expect(!public_header_includes_flycube("rhi.h"),
         "public rhi.h must not include FlyCube");
  expect(wrap_tu_includes_flycube("flycube/device.cc"),
         "wrap TU flycube/device.cc may include FlyCube");

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
      render::rhi::Pipeline* lit =
          dx12->create_graphics_pipeline(render::programs::lit_pipeline_desc());
      expect(lit != nullptr, "lit pipeline");
      const render::programs::Color clear_tint{0.2f, 0.4f, 0.6f, 1.f};
      plist->set_pipeline(lit);
      plist->set_constants(render::programs::kColorSlot, &clear_tint,
                           static_cast<uint32_t>(sizeof(clear_tint)));

      // Lit solid smoke — pos+normal triangle through the shared lit program.
      const float lit_verts[] = {
          0.f,  0.5f, 0.f, 0.f, 0.f, 1.f,   // tip
          -0.5f, -0.5f, 0.f, 0.f, 0.f, 1.f,  // BL
          0.5f, -0.5f, 0.f, 0.f, 0.f, 1.f,   // BR
      };
      const uint32_t lit_indices[] = {0u, 1u, 2u};
      render::rhi::Buffer* lit_vb = dx12->create_buffer(
          sizeof(lit_verts), render::rhi::BufferUsage::kVertex);
      render::rhi::Buffer* lit_ib = dx12->create_buffer(
          sizeof(lit_indices), render::rhi::BufferUsage::kIndex);
      expect(lit_vb != nullptr && lit_ib != nullptr, "lit buffers");
      expect(dx12->upload(lit_vb, lit_verts, sizeof(lit_verts)),
             "upload lit vb");
      expect(dx12->upload(lit_ib, lit_indices, sizeof(lit_indices)),
             "upload lit ib");
      render::rhi::CameraMatrices lit_cam =
          render::rhi::make_orbit_camera(0.f, 0.35f, 3.2f, 0.785398f,
                                         1.f /* aspect */, 0.1f, 100.f);
      plist->bind_camera(lit_cam);
      render::programs::Light gpu_light{};
      gpu_light.dir_x = -0.4f;
      gpu_light.dir_y = -0.8f;
      gpu_light.dir_z = -0.35f;
      gpu_light.ambient = 0.25f;
      gpu_light.color_r = 1.f;
      gpu_light.color_g = 0.95f;
      gpu_light.color_b = 0.9f;
      gpu_light.intensity = 1.1f;
      plist->set_constants(render::programs::kLightSlot, &gpu_light,
                           static_cast<uint32_t>(sizeof(gpu_light)));
      const render::programs::Color albedo{0.35f, 0.65f, 0.9f, 1.f};
      plist->set_constants(render::programs::kColorSlot, &albedo,
                           static_cast<uint32_t>(sizeof(albedo)));
      plist->bind_vertex_buffer(lit_vb, 0, 6u * sizeof(float));
      plist->bind_index_buffer(lit_ib, 0);
      plist->draw_indexed(3, 1, 0, 0, 0);

      plist->end_render_pass();
      plist->close();
      expect(dx12->execute(plist), "dx12 clear+lit execute");
      dx12->present();
      std::fprintf(stdout, "rhi_test: dx12 present ok\n");
      std::fprintf(stdout, "rhi_test: lit ok\n");
      // Release GPU buffers before device shutdown (avoids teardown races).
      dx12->destroy_pipeline(lit);
      dx12->destroy_buffer(lit_ib);
      dx12->destroy_buffer(lit_vb);
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
