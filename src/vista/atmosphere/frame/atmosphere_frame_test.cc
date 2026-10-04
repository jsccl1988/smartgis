// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/atmosphere/frame/atmosphere_frame.h"

#include <cstdio>
#include <memory>

#include "vista/atmosphere/cloud/cloud_pass.h"
#include "vista/atmosphere/fog/fog_pass.h"
#include "vista/atmosphere/ocean/ocean_pass.h"
#include "vista/atmosphere/sky/sky_pass.h"
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
  using vista::AtmosphereFrame;
  using vista::CloudPass;
  using vista::FogPass;
  using vista::OceanPass;
  using vista::SkyPass;
  using render::rhi::Backend;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  AtmosphereFrame frame;
  expect(frame.clears_color(), "default clears_color true (opaque depth chain)");
  expect(frame.uses_shared_depth(), "default shared depth true");
  expect(!frame.sky_enabled() && !frame.fog_enabled(),
         "default sky/fog off");

  std::unique_ptr<render::rhi::Device> device(
      create_device(Backend::kNull));
  expect(device != nullptr, "create Null device");
  if (!device) {
    return 1;
  }
  render::rhi::CommandList* list = device->create_command_list();
  expect(list != nullptr, "create command list");
  if (!list) {
    return 1;
  }

  // ocean enabled, no pass �?fail closed
  frame.set_ocean_enabled(true);
  expect(frame.clears_color(), "ocean clears color");
  expect(!frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "ocean enabled nullptr pass fails");

  // ocean disabled �?no-op success
  frame.set_ocean_enabled(false);
  expect(frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "ocean disabled no-op");

  // ocean with pass �?record succeeds (Null)
  OceanPass ocean;
  frame.set_ocean_pass(&ocean);
  frame.set_ocean_enabled(true);
  expect(frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "ocean with pass records");

  // cloud enabled, no pass �?fail closed
  frame.set_cloud_enabled(true);
  frame.set_cloud_pass(nullptr);
  expect(!frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "cloud enabled nullptr pass fails");

  frame.set_cloud_enabled(false);
  expect(frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "cloud disabled no-op");

  CloudPass cloud;
  frame.set_cloud_pass(&cloud);
  frame.set_cloud_enabled(true);
  expect(frame.uses_shared_depth(), "cloud uses shared depth");
  expect(frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "cloud with pass records");

  // sky enabled, no pass �?fail closed
  frame.set_ocean_enabled(false);
  frame.set_cloud_enabled(false);
  frame.set_sky_enabled(true);
  expect(frame.clears_color(), "sky clears color");
  expect(!frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "sky enabled nullptr pass fails");

  SkyPass sky;
  frame.set_sky_pass(&sky);
  expect(frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "sky with pass records");

  // sky + ocean share one pre-opaque pass
  frame.set_ocean_enabled(true);
  expect(frame.record_pre_opaque(device.get(), list, 64, 64, nullptr),
         "sky+ocean pre-opaque");

  // fog enabled, no pass �?fail closed
  frame.set_fog_enabled(true);
  expect(!frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "fog enabled nullptr pass fails");

  FogPass fog;
  frame.set_fog_pass(&fog);
  frame.set_cloud_enabled(false);
  expect(frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "fog with pass records");

  // cloud + fog post-opaque
  frame.set_cloud_enabled(true);
  expect(frame.record_post_opaque(device.get(), list, 64, 64, nullptr, 1),
         "cloud+fog post-opaque");

  device->destroy_command_list(list);

  if (g_fails != 0) {
    std::fprintf(stderr, "atmosphere_frame_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("atmosphere_frame_test: PASS\n");
  return 0;
}
