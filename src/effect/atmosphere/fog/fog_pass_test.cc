// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/fog/fog_pass.h"

#include <cstdio>
#include <memory>

#include "effect/atmosphere/fog/constants.h"
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
  using effect::atmosphere::FogDrawParams;
  using effect::atmosphere::FogPass;
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
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
  expect(far_f > near_f, "farther â†?denser fog");
  expect(far_f > high_f, "higher altitude â†?less fog");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  FogPass fog;
  fog.set_params(p);

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");
  const auto cam = make_orbit_camera(0.f, 0.4f, 3.2f, 0.8f, 1.f, 0.1f, 100.f);
  expect(fog.record(device.get(), list, 64, 64, &cam), "fog record");

  auto* stub = dynamic_cast<StubCommandList*>(list);
  expect(stub != nullptr, "stub list");
  if (stub) {
    expect(stub->last_pipeline == fog.pipeline() && fog.pipeline() != nullptr,
           "fog dedicated pipeline");
    const auto* fog_cb = stub->constant_at(1);
    expect(fog_cb != nullptr &&
               fog_cb->byte_size == sizeof(effect::atmosphere::FogConstants),
           "fog FogCB slot 1");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "fog alpha blend");
    expect(stub->last_depth == render::rhi::DepthMode::kDisabled,
           "fog fullscreen haze skips depth");
    expect(stub->draw_indexed_calls >= 1u, "fog drew indexed");
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
