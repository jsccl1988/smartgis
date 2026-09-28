// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/graph/frame_graph.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "effect/map/map_effect.h"
#include "effect/map/pass.h"
#include "render/rhi/rhi.h"

namespace {

int g_fails = 0;

class CountingEffect : public render::graph::Effect {
 public:
  CountingEffect(render::graph::EffectSlot slot, int* calls,
                 bool clears_color = false)
      : slot_(slot), calls_(calls), clears_color_(clears_color) {}

  render::graph::EffectSlot slot() const override { return slot_; }
  bool clears_color() const override { return clears_color_; }

  bool record(const render::graph::RecordContext& ctx) override {
    color_op_ = ctx.color_op;
    if (calls_) {
      ++*calls_;
    }
    return true;
  }

  render::rhi::ColorLoadOp color_op() const { return color_op_; }

 private:
  render::graph::EffectSlot slot_;
  int* calls_ = nullptr;
  bool clears_color_ = false;
  render::rhi::ColorLoadOp color_op_ = render::rhi::ColorLoadOp::kClear;
};

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

gis::vista::View view_64() {
  gis::vista::View view;
  view.width_px = 64;
  view.height_px = 64;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = 10;
  view.max_y = 10;
  return view;
}

}  // namespace

int main() {
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device && device->initialize(DeviceDesc()), "null device");

  render::graph::ViewInput empty;
  empty.width_px = 64;
  empty.height_px = 64;
  expect(render::graph::present(device.get(), empty), "empty effects present");
  expect(!render::graph::present(nullptr, empty), "null device fails");
  empty.width_px = 0;
  expect(!render::graph::present(device.get(), empty), "zero width fails");

  int before_calls = 0;
  int opaque_calls = 0;
  int after_calls = 0;
  int overlay_calls = 0;
  CountingEffect before(render::graph::EffectSlot::kBeforeOpaque, &before_calls,
                        true);
  CountingEffect opaque(render::graph::EffectSlot::kOpaque, &opaque_calls);
  CountingEffect after(render::graph::EffectSlot::kAfterOpaque, &after_calls);
  CountingEffect overlay(render::graph::EffectSlot::kOverlay, &overlay_calls);
  render::graph::ViewInput slots;
  slots.width_px = 64;
  slots.height_px = 64;
  // Vector order is not slot order. Overlay is first so a slot walk is
  // what makes it observe kLoad after the before-effect clears.
  slots.effects.push_back(&overlay);
  slots.effects.push_back(&after);
  slots.effects.push_back(&opaque);
  slots.effects.push_back(&before);
  expect(render::graph::present(device.get(), slots), "four slots present");
  expect(before_calls == 1, "before-effect recorded once");
  expect(opaque_calls == 1, "opaque-effect recorded once");
  expect(after_calls == 1, "after-effect recorded once");
  expect(overlay_calls == 1, "overlay-effect recorded once");
  expect(before.color_op() == render::rhi::ColorLoadOp::kClear,
         "before-effect starts on clear");
  expect(opaque.color_op() == render::rhi::ColorLoadOp::kLoad,
         "later effect sees load after a color clear");
  expect(after.color_op() == render::rhi::ColorLoadOp::kLoad,
         "after-effect sees load");
  expect(overlay.color_op() == render::rhi::ColorLoadOp::kLoad,
         "overlay-effect sees load");

  effect::map::Pass pass;
  const gis::vista::View view = view_64();
  gis::vista::MapFrame frame;
  frame.background_rgba = 0xff1b3a4c;
  const render::rhi::CameraMatrices ortho = render::rhi::make_ortho_camera(
      0.f, 10.f, 0.f, 10.f, -1.f, 1.f);
  effect::map::MapEffect map_effect(render::graph::EffectSlot::kOpaque, &pass,
                                      &frame, &view, nullptr, {}, {}, true);
  expect(map_effect.slot() == render::graph::EffectSlot::kOpaque,
         "record_all stays on the opaque slot");
  expect(ortho.kind == render::rhi::CameraKind::kOrtho, "map camera is ortho");
  render::graph::ViewInput map_only;
  map_only.width_px = 64;
  map_only.height_px = 64;
  map_only.camera = &ortho;
  map_only.effects.push_back(&map_effect);
  expect(render::graph::present(device.get(), map_only),
         "map record_all presents");

  device->shutdown();
  if (g_fails) {
    std::fprintf(stderr, "frame_graph_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "frame_graph_test: ok\n");
  std::fflush(stdout);
  std::_Exit(0);
}
