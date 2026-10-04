// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/atmosphere/sky/sky_pass.h"

#include <cmath>
#include <cstdio>
#include <memory>

#include "vista/atmosphere/sky/constants.h"
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
  using vista::SkyDrawParams;
  using vista::SkyPass;
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::RenderPassDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;
  using render::rhi::make_orbit_camera;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  SkyDrawParams p;
  p.sun_y = 0.9f;
  float zr = 0.f;
  float zg = 0.f;
  float zb = 0.f;
  float hr = 0.f;
  float hg = 0.f;
  float hb = 0.f;
  SkyPass::sample_sky_rgb(p, 0.f, 1.f, 0.f, &zr, &zg, &zb);
  SkyPass::sample_sky_rgb(p, 0.f, 0.f, 1.f, &hr, &hg, &hb);
  // Zenith is deeper/cooler (Bruneton-lite Rayleigh); horizon brighter under
  // high sun (haze + less Rayleigh deepen).
  expect(zr < hr, "zenith less red than horizon under high sun");
  expect(zb > zr, "zenith cooler (more blue than red) under high sun");
  expect((zr + zg + zb) < (hr + hg + hb),
         "zenith darker than brightened horizon under high sun");

  SkyDrawParams sunset = p;
  sunset.sun_y = 0.05f;
  float sr = 0.f;
  float sg = 0.f;
  float sb = 0.f;
  // Same elev_v as the high-sun horizon sample so warm is isolated from blend.
  SkyPass::sample_sky_rgb(sunset, 0.f, 0.f, 1.f, &sr, &sg, &sb);
  expect(sr > hr, "low sun warms horizon");
  // Cool ground clamp keeps blue; warm must not invent G≈0 magenta.
  expect(sb > hb * 0.85f, "twilight horizon keeps blue/purple bias");
  expect(sg > 0.20f, "warmed horizon keeps green (no magenta)");

  SkyDrawParams space = p;
  space.dome_radius = -40.0f;
  float spr = 0.f;
  float spg = 0.f;
  float spb = 0.f;
  SkyPass::sample_sky_rgb(space, 0.f, 1.f, 0.f, &spr, &spg, &spb);
  expect((spr + spg + spb) < 0.12f, "space zenith is deep/dark");
  expect(spb >= spr && spb >= spg, "space tint stays cool blue");

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  SkyPass sky;
  sky.set_sun_from_azimuth_elevation(0.4f, 0.7f);

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");

  RenderPassDesc pass;
  pass.width = 64;
  pass.height = 64;
  pass.load_op = render::rhi::ColorLoadOp::kClear;
  pass.enable_depth = true;
  list->begin_render_pass(pass);
  const auto cam = make_orbit_camera(0.f, 0.4f, 3.2f, 0.8f, 1.f, 0.1f, 100.f);
  expect(sky.record(device.get(), list, 64, 64, &cam), "sky record");
  list->end_render_pass();

  auto* stub = dynamic_cast<StubCommandList*>(list);
  expect(stub != nullptr, "stub list");
  if (stub) {
    expect(stub->last_pipeline == sky.pipeline() && sky.pipeline() != nullptr,
           "sky HLSL pipeline");
    const auto* sky_cb = stub->constant_at(1);
    expect(sky_cb != nullptr &&
               sky_cb->byte_size == sizeof(vista::SkyConstants),
           "sky SkyCB slot 1");
    expect(stub->draw_indexed_calls >= 1u, "sky drew indexed");
  }

  device->destroy_command_list(list);
  sky.release();

  if (g_fails != 0) {
    std::fprintf(stderr, "sky_pass_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("sky_pass_test: PASS\n");
  return 0;
}
