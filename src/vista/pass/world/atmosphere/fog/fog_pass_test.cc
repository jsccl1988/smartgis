// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/fog/fog_pass.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include "vista/pass/world/atmosphere/fog/constants.h"
#include "render/rhi/rhi.h"

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
  using vista::FogConstants;
  using vista::FogDrawParams;
  using vista::FogPass;
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::TextureDesc;
  using render::rhi::create_device;
  using render::rhi::make_orbit_camera;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  FogDrawParams p;
  p.density = 0.5f;
  p.visibility = 4.0f;
  p.height_falloff = 2.0f;
  p.max_opacity = 1.0f;
  const float near_f = FogPass::fog_factor(p, 0.5f, 0.0f);
  const float far_f = FogPass::fog_factor(p, 8.0f, 0.0f);
  const float high_f = FogPass::fog_factor(p, 8.0f, 3.0f);
  expect(far_f > near_f, "farther -> denser fog");
  expect(far_f > high_f, "higher altitude -> less fog");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  FogPass fog;
  fog.set_params(p);

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");
  const auto cam = make_orbit_camera(0.f, 0.4f, 3.2f, 0.8f, 1.f, 0.1f, 100.f);

  // Far-ray path: no depth SRV, CameraCB still bound.
  expect(fog.record(device.get(), list, 64, 64, &cam), "fog record (no depth)");

  auto* stub = dynamic_cast<StubCommandList*>(list);
  expect(stub != nullptr, "stub list");
  if (stub) {
    expect(stub->last_pipeline == fog.pipeline() && fog.pipeline() != nullptr,
           "fog dedicated pipeline");
    const auto* fog_cb = stub->constant_at(1);
    expect(fog_cb != nullptr && fog_cb->byte_size == sizeof(FogConstants),
           "fog FogCB slot 1");
    if (fog_cb && fog_cb->has_bytes) {
      FogConstants decoded{};
      std::memcpy(&decoded, fog_cb->bytes, sizeof(decoded));
      expect(decoded.use_depth < 0.5f, "use_depth off without SRV");
    }
    expect(stub->bind_camera_calls >= 1u, "CameraCB bound (slot 0)");
    expect(stub->bind_texture_calls == 0u, "no depth bind when null");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "fog alpha blend");
    expect(stub->last_depth == render::rhi::DepthMode::kDisabled,
           "fog fullscreen haze skips depth");
    expect(stub->draw_indexed_calls >= 1u, "fog drew indexed");
  }

  // Depth path: optional SRV enables use_depth and bind_texture(slot 0).
  TextureDesc depth_desc;
  depth_desc.width = 64;
  depth_desc.height = 64;
  render::rhi::Texture* depth_tex = device->create_texture(depth_desc);
  expect(depth_tex != nullptr, "stub depth texture");
  const uint32_t draws_before = stub ? stub->draw_indexed_calls : 0;
  const uint32_t tex_before = stub ? stub->bind_texture_calls : 0;
  expect(fog.record(device.get(), list, 64, 64, &cam, depth_tex),
         "fog record (with depth)");
  if (stub) {
    expect(stub->bind_texture_calls == tex_before + 1u, "depth bind once");
    expect(stub->last_texture == depth_tex, "bound depth pointer");
    expect(stub->draw_indexed_calls == draws_before + 1u,
           "depth path drew indexed");
    const auto* fog_cb = stub->constant_at(1);
    expect(fog_cb != nullptr && fog_cb->has_bytes, "FogCB after depth record");
    if (fog_cb && fog_cb->has_bytes) {
      FogConstants decoded{};
      std::memcpy(&decoded, fog_cb->bytes, sizeof(decoded));
      expect(decoded.use_depth > 0.5f, "use_depth on with SRV");
    }
  }
  if (depth_tex) {
    device->destroy_texture(depth_tex);
  }

  device->destroy_command_list(list);
  fog.release();

  if (g_fails != 0) {
    std::fprintf(stderr, "fog_pass_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("fog_pass_test: PASS\n");
  return 0;
}
