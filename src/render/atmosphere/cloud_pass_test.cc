// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/atmosphere/cloud_pass.h"

#include <cmath>
#include <cstdio>
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
  using render::atmosphere::CloudPass;
  using render::atmosphere::CloudRayInput;
  using render::rhi::Backend;
  using render::rhi::CameraMatrices;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  expect(CloudPass::step_count_for_quality(0) == 8, "steps q0");
  expect(CloudPass::step_count_for_quality(1) == 16, "steps q1");
  expect(CloudPass::step_count_for_quality(3) == 64, "steps q3");

  expect_near(CloudPass::beer_transmittance(0.0f), 1.0f, 1.0e-6f, "beer 0");
  expect(CloudPass::beer_transmittance(1.0f) < 0.4f, "beer 1 decays");
  expect(CloudPass::beer_transmittance(10.0f) < 1.0e-3f, "beer thick");

  // Empty cover → no luminance, full transmittance.
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

  // Full cover ray through slab → some scatter, T < 1.
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
    expect(pass.record(device.get(), list, 128, 72, &cam, 2), "record ok");

    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->draw_indexed_calls >= 1, "deck draw");
    expect(stub->index_counts.size() >= 1 && stub->index_counts[0] == 6,
           "quad 6 indices");
    expect(stub->bind_camera_calls >= 1, "bind camera");
    expect(stub->set_solid_color_calls >= 1, "solid encodes luminance");
    expect(stub->begin_render_pass_calls >= 1, "load pass marker");
    expect(stub->clear_load_calls == 0, "cloud must not clear");
    expect(stub->load_load_calls >= 1, "cloud uses ColorLoadOp::kLoad");
    expect(stub->last_load_op == render::rhi::ColorLoadOp::kLoad,
           "last load op is Load");
    expect(stub->last_pipeline == render::rhi::PipelineId::kCloud,
           "cloud pipeline");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "cloud alpha blend");
    expect(stub->last_depth == render::rhi::DepthMode::kTestOnly,
           "cloud depth test-only");
    expect(stub->set_cloud_params_calls >= 1, "cloud gpu params");
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
