// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/atmosphere/cloud/cloud_pass.h"
#include "effect/atmosphere/cloud/constants.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "render/rhi/rhi.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void expect_near(float got, float want, float eps, const char* msg) {
  if (std::fabs(got - want) > eps) {
    std::fprintf(stderr, "FAIL: %s (got=%g want=%g)\n", msg, got, want);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using effect::atmosphere::CloudPass;
  using effect::atmosphere::CloudRayInput;
  using render::rhi::Backend;
  using render::rhi::CameraMatrices;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  expect(CloudPass::step_count_for_quality(0) == 8, "steps q0");
  expect(CloudPass::step_count_for_quality(1) == 16, "steps q1");
  expect(CloudPass::step_count_for_quality(2) == 32, "steps q2");
  expect(CloudPass::step_count_for_quality(3) == 64, "steps q3");

  expect(CloudPass::uses_half_res_proxy(0), "proxy q0");
  expect(CloudPass::uses_half_res_proxy(1), "proxy q1");
  expect(!CloudPass::uses_half_res_proxy(2), "no proxy q2");
  expect(CloudPass::density_cell_for_quality(0) > 0.0f, "cell q0");
  expect(CloudPass::density_cell_for_quality(1) > 0.0f, "cell q1");
  expect_near(CloudPass::density_cell_for_quality(2), 0.0f, 1.0e-6f, "cell q2");
  expect_near(CloudPass::density_cell_for_quality(3), 0.0f, 1.0e-6f, "cell q3");

  expect_near(CloudPass::beer_transmittance(0.0f), 1.0f, 1.0e-6f, "beer 0");
  expect(CloudPass::beer_transmittance(1.0f) < 0.4f, "beer 1 decays");
  expect(CloudPass::beer_transmittance(10.0f) < 1.0e-3f, "beer thick");

  // Powder = 1 - exp(-dens * 8): thin mid-band, thick saturates to 1.
  expect_near(CloudPass::powder_factor(0.0f), 0.0f, 1.0e-6f, "powder 0");
  {
    const float thin = CloudPass::powder_factor(0.05f);
    expect(thin > 0.3f && thin < 0.4f, "powder thin mid");
  }
  expect(CloudPass::powder_factor(2.0f) > 0.99f, "powder thick ~1");

  expect_near(CloudPass::silver_lining(0.0f), 0.0f, 1.0e-6f, "silver 0");
  expect_near(CloudPass::silver_lining(1.0f), 1.0f, 1.0e-6f, "silver 1");
  expect(CloudPass::silver_lining(0.5f) < CloudPass::silver_lining(0.9f),
         "silver rises with toward-sun");

  // Empty cover: no luminance, full transmittance.
  {
    CloudRayInput in;
    in.origin_y = 0.0f;
    in.dir_y = 1.0f;
    in.base_y = 1000.0f;
    in.top_y = 3000.0f;
    in.cover = 0.0f;
    in.steps = 16;
    const auto r = CloudPass::march_ray(in);
    expect_near(r.luminance, 0.0f, 1.0e-6f, "empty luminance");
    expect_near(r.transmittance, 1.0f, 1.0e-6f, "empty T");
  }

  // Full cover ray through slab: some scatter, T < 1.
  {
    CloudRayInput in;
    in.origin_x = 0.0f;
    in.origin_y = 0.0f;
    in.origin_z = 0.0f;
    in.dir_x = 0.0f;
    in.dir_y = 1.0f;
    in.dir_z = 0.0f;
    in.sun_x = 0.0f;
    in.sun_y = 0.7071f;
    in.sun_z = 0.7071f;
    in.base_y = 1000.0f;
    in.top_y = 3000.0f;
    in.cover = 1.0f;
    in.extinction = 0.05f;
    in.steps = 32;
    const auto r = CloudPass::march_ray(in);
    expect(r.luminance > 0.0f, "scatter luminance");
    expect(r.transmittance < 1.0f, "beer attenuates");
    expect(r.transmittance > 0.0f, "T positive");
  }

  // Silver is applied in march_ray; path shadows can dominate luminance, so
  // formula coverage stays on silver_lining() / powder_factor() above.
  {
    CloudRayInput in;
    in.origin_y = 0.0f;
    in.dir_y = 1.0f;
    in.sun_x = 0.0f;
    in.sun_y = 1.0f;
    in.sun_z = 0.0f;
    in.base_y = 1000.0f;
    in.top_y = 3000.0f;
    in.cover = 1.0f;
    in.extinction = 0.05f;
    in.steps = 32;
    const auto r = CloudPass::march_ray(in);
    expect(r.luminance > 0.0f, "powder+silver path luminance");
    expect(r.transmittance < 1.0f, "beer still attenuates");
  }

  // Half-res proxy coarse snap still integrates without crashing.
  {
    CloudRayInput in;
    in.origin_y = 0.0f;
    in.dir_y = 1.0f;
    in.base_y = 1000.0f;
    in.top_y = 3000.0f;
    in.cover = 1.0f;
    in.extinction = 0.05f;
    in.steps = CloudPass::step_count_for_quality(0);
    in.density_cell = CloudPass::density_cell_for_quality(0);
    const auto r = CloudPass::march_ray(in);
    expect(r.luminance >= 0.0f, "proxy luminance non-neg");
    expect(r.transmittance > 0.0f && r.transmittance <= 1.0f, "proxy T");
  }

  // Null RHI smoke: record does not crash and issues a fullscreen draw.
  {
    std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
    expect(device != nullptr, "null device");
    expect(device->initialize(DeviceDesc()), "null init");

    CloudPass pass;
    pass.set_sun_from_azimuth_elevation(0.5f, 0.7f);
    pass.set_cloud_slab(800.0f, 2500.0f);
    pass.set_cover_modulation(0.8f);

    expect(!pass.record(nullptr, nullptr, 64, 64, nullptr, 1), "null args");
    expect(!pass.record(device.get(), nullptr, 64, 64, nullptr, 1),
           "null list");

    render::rhi::CommandList* list = device->create_command_list();
    expect(list != nullptr, "cmd list");
    CameraMatrices cam = render::rhi::make_perspective_camera(
        0.785398f, 1.0f, 0.1f, 10000.0f);
    // quality 1 exercises half-res proxy constants; quality 2 full detail.
    expect(pass.record(device.get(), list, 128, 72, &cam, 1), "record q1");
    {
      auto* stub = static_cast<StubCommandList*>(list);
      const auto* cloud_cb = stub->constant_at(1);
      expect(cloud_cb != nullptr &&
                 cloud_cb->byte_size ==
                     sizeof(effect::atmosphere::CloudConstants),
             "cloud constants slot 1");
      if (cloud_cb && cloud_cb->has_bytes) {
        effect::atmosphere::CloudConstants cb{};
        std::memcpy(&cb, cloud_cb->bytes, sizeof(cb));
        expect(cb.steps == 16.0f, "q1 steps in CB");
        expect(cb.density_cell > 0.0f, "q1 density_cell in CB");
      }
    }

    expect(pass.record(device.get(), list, 128, 72, &cam, 2), "record q2");

    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->draw_indexed_calls >= 2, "deck draws");
    expect(stub->index_counts.size() >= 1 && stub->index_counts[0] == 6,
           "quad 6 indices");
    expect(stub->bind_camera_calls >= 1, "bind camera");
    const auto* cloud_cb = stub->constant_at(1);
    expect(cloud_cb != nullptr &&
               cloud_cb->byte_size == sizeof(effect::atmosphere::CloudConstants),
           "cloud constants after q2");
    if (cloud_cb && cloud_cb->has_bytes) {
      effect::atmosphere::CloudConstants cb{};
      std::memcpy(&cb, cloud_cb->bytes, sizeof(cb));
      expect(cb.steps == 32.0f, "q2 steps in CB");
      expect_near(cb.density_cell, 0.0f, 1.0e-6f, "q2 density_cell zero");
    }
    expect(stub->begin_render_pass_calls >= 1, "load pass marker");
    expect(stub->clear_load_calls == 0, "cloud must not clear");
    expect(stub->load_load_calls >= 1, "cloud uses ColorLoadOp::kLoad");
    expect(stub->last_load_op == render::rhi::ColorLoadOp::kLoad,
           "last load op is Load");
    expect(stub->last_pipeline == pass.pipeline() && pass.pipeline() != nullptr,
           "cloud pipeline");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "cloud alpha blend");
    expect(stub->last_depth == render::rhi::DepthMode::kTestOnly,
           "cloud depth test-only");
    expect(stub->set_constants_calls >= 1, "cloud constants");
    expect(stub->depth_enabled_pass_calls >= 1, "cloud enables depth attach");
    expect(!stub->closed, "pass leaves list open");

    list->close();
    expect(device->execute(list), "execute");

    pass.release();
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d cloud_pass check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "cloud_pass_test OK\n");
  return 0;
}
