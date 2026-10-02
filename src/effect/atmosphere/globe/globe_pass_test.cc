// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/globe/globe_pass.h"
#include "effect/atmosphere/globe/sat_cloud_pass.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

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
  using effect::atmosphere::GlobeDrawParams;
  using effect::atmosphere::GlobePass;
  using effect::atmosphere::SatCloudPass;
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::RenderPassDesc;
  using render::rhi::create_device;
  using render::rhi::make_orbit_camera;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  // Tiny china-window DEM (proves regional fallback on full sphere).
  constexpr int kCols = 8;
  constexpr int kRows = 6;
  std::vector<float> heights(static_cast<size_t>(kCols * kRows), 0.f);
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      heights[static_cast<size_t>(r * kCols + c)] =
          200.f + 800.f * static_cast<float>(c) / (kCols - 1);
    }
  }

  GlobePass globe;
  GlobeDrawParams gp;
  gp.lon_slices = 24;
  gp.lat_slices = 12;
  globe.set_params(gp);
  globe.set_dem_surface(73.0, 18.0, 135.0, 54.0, kCols, kRows, heights.data(),
                        heights.size(), nullptr, 0, 0);
  expect(globe.has_surface(), "globe has china surface");
  expect(!globe.dem_is_global(), "china window is not global");

  SatCloudPass clouds;
  clouds.seed_procedural_cover(64, 32, 3);
  expect(clouds.has_cover(), "sat cloud procedural cover");

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");
  const render::rhi::CameraMatrices cam =
      make_orbit_camera(1.2f, 0.35f, 2.8f, 1.0f, 4.f / 3.f, 0.1f, 100.f);

  RenderPassDesc pass;
  pass.width = 64;
  pass.height = 64;
  pass.enable_depth = true;
  pass.depth_clear = 1.f;
  list->begin_render_pass(pass);
  expect(globe.record(device.get(), list, 64, 64, &cam), "globe record");
  list->end_render_pass();
  expect(clouds.record(device.get(), list, 64, 64, &cam), "sat cloud record");
  list->close();
  expect(device->execute(list), "execute");
  device->destroy_command_list(list);

  globe.release();
  clouds.release();
  device->shutdown();

  if (g_fails != 0) {
    std::fprintf(stderr, "globe_pass_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "globe_pass_test: PASS\n");
  return 0;
}
