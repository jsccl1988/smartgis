// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/atmosphere/field_texture.h"
#include "render/atmosphere/ocean_pass.h"

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
  using render::atmosphere::FieldTexture;
  using render::atmosphere::OceanDrawParams;
  using render::atmosphere::OceanPass;
  using render::rhi::Backend;
  using render::rhi::CameraMatrices;
  using render::rhi::DeviceDesc;
  using render::rhi::RenderPassDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;
  using render::rhi::make_perspective_camera;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "null device");
  expect(device->initialize(DeviceDesc()), "initialize");

  // FieldTexture upload smoke.
  FieldTexture mask_tex;
  const int mc = 4;
  const int mr = 4;
  std::vector<float> mask(static_cast<std::size_t>(mc * mr), 1.0f);
  // Land strip on left column.
  for (int r = 0; r < mr; ++r) {
    mask[static_cast<std::size_t>(r * mc + 0)] = 0.0f;
  }
  expect(mask_tex.upload(device.get(), mc, mr, mask.data(), mask.size(), 1.0f),
         "mask upload");
  expect(!mask_tex.empty(), "mask not empty");
  expect(mask_tex.texture() != nullptr, "mask texture");

  OceanPass ocean;
  OceanDrawParams params;
  params.significant_wave_height = 1.2f;
  params.mean_direction_rad = 0.3f;
  params.wind_speed = 8.0f;
  params.wind_direction_rad = 0.3f;
  params.fft_size = 32;
  params.use_gerstner_fallback = false;
  params.mesh_resolution = 17;
  params.patch_half_extent = 20.0f;
  ocean.set_params(params);
  ocean.set_time_sec(1.25);
  ocean.set_sea_mask_cpu(mc, mr, mask.data(), mask.size());
  ocean.set_sea_mask_texture(&mask_tex);

  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "command list");

  RenderPassDesc pass;
  pass.width = 64;
  pass.height = 64;
  list->begin_render_pass(pass);

  CameraMatrices cam = make_perspective_camera(0.8f, 1.0f, 0.1f, 500.0f);
  expect(ocean.record(device.get(), list, 64, 64, &cam), "fft record");
  // Energy-normalized encode scale tracks FieldStore Hs (≈ 0.55 * Hs).
  expect(std::fabs(ocean.height_scale() - 1.2f * 0.55f) < 0.05f,
         "height_scale ~ 0.55*Hs");
  expect(ocean.disp_scale() > 0.1f, "disp_scale positive");

  // Phillips fallback still records.
  params.use_jonswap = false;
  ocean.set_params(params);
  expect(ocean.record(device.get(), list, 64, 64, &cam), "phillips record");
  params.use_jonswap = true;
  ocean.set_params(params);

  // Gerstner fallback path.
  params.use_gerstner_fallback = true;
  params.fft_size = 0;
  ocean.set_params(params);
  expect(ocean.record(device.get(), list, 64, 64, &cam), "gerstner record");

  list->end_render_pass();
  list->close();
  expect(device->execute(list), "execute");

  auto* stub = static_cast<StubCommandList*>(list);
  expect(stub->draw_indexed_calls >= 1, "drew ocean");
  expect(stub->set_solid_color_calls >= 1, "fresnel tint");
  expect(stub->bind_vertex_calls >= 1, "bind vb");
  expect(stub->bind_index_calls >= 1, "bind ib");
  expect(stub->set_pipeline_calls >= 1, "ocean pipeline");
  expect(stub->last_pipeline == render::rhi::PipelineId::kOcean, "pipeline id");
  expect(stub->last_depth == render::rhi::DepthMode::kWrite, "ocean depth write");
  expect(stub->set_ocean_params_calls >= 1, "ocean params");
  expect(stub->bind_texture_calls >= 1, "height texture");
  expect(!ocean.used_gpu_fft(), "null falls back to CPU FFT");
  expect(!device->supports_compute(), "null has no compute");

  // All-land mask → no crash, zero draws for that call (still ok).
  OceanPass land_only;
  OceanDrawParams calm = params;
  calm.mesh_resolution = 5;
  land_only.set_params(calm);
  std::vector<float> land(25, 0.0f);
  land_only.set_sea_mask_cpu(5, 5, land.data(), land.size());
  render::rhi::CommandList* list2 = device->create_command_list();
  list2->begin_render_pass(pass);
  expect(land_only.record(device.get(), list2, 64, 64, &cam), "land discard");
  list2->end_render_pass();
  list2->close();

  ocean.release();
  land_only.release();
  mask_tex.release();

  if (g_fails != 0) {
    std::fprintf(stderr, "ocean_pass_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "ocean_pass_test: ok\n");
  return 0;
}
