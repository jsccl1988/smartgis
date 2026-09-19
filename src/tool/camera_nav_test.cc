// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/camera_nav.h"

#include <cmath>
#include <cstdio>

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
  expect(std::fabs(tool::wheel_zoom_factor(0) - 1.0) < 1e-12, "zero wheel");
  expect(tool::wheel_zoom_factor(120) > 1.05, "positive wheel zooms in");
  expect(tool::wheel_zoom_factor(-120) < 0.95, "negative wheel zooms out");
  expect(std::fabs(tool::wheel_zoom_factor(120) *
                   tool::wheel_zoom_factor(-120) -
                   1.0) < 1e-9,
         "in/out cancel");

  expect(tool::pinch_to_wheel_delta(100.0, 110.0) > 0, "pinch out");
  expect(tool::pinch_to_wheel_delta(110.0, 100.0) < 0, "pinch in");
  expect(tool::pinch_to_wheel_delta(100.0, 100.2) == 0, "deadzone");
  expect(tool::pinch_to_wheel_delta(1.0, 1.2) == 0, "unit dist is pixel guard");
  expect(tool::scale_to_wheel_delta(1.2) > 0, "scale-out dollies in");
  expect(tool::scale_to_wheel_delta(0.8) < 0, "scale-in dollies out");

  {
    double pan_x = 10;
    double pan_y = 20;
    double scale = 2.0;
    // Map point under (50, 80) is (20, 30).
    tool::zoom_at_client_point(&pan_x, &pan_y, &scale, 50, 80, 2.0);
    expect(std::fabs(scale - 4.0) < 1e-9, "scale doubles");
    const double mx = (50.0 - pan_x) / scale;
    const double my = (80.0 - pan_y) / scale;
    expect(std::fabs(mx - 20.0) < 1e-6, "cursor map-x fixed");
    expect(std::fabs(my - 30.0) < 1e-6, "cursor map-y fixed");
  }

  {
    const tool::BlitDestRect d =
        tool::zoom_blit_dest(800, 600, 200, 150, 2.0);
    expect(d.w == 1600 && d.h == 1200, "zoom-in dest grows");
    expect(d.x == -200 && d.y == -150, "scale origin is cursor");
    const tool::BlitDestRect out =
        tool::zoom_blit_dest(800, 600, 200, 150, 0.5);
    expect(out.w == 400 && out.h == 300, "zoom-out dest shrinks");
    expect(out.x == 100 && out.y == 75, "zoom-out origin is cursor");
    expect(tool::kBlitDebounceMs >= 150 && tool::kBlitDebounceMs <= 250,
           "debounce 150-250ms");
  }

  {
    const tool::WorldExtent last{100.0, 20.0, 120.0, 40.0};
    const tool::WorldExtent next =
        tool::zoom_world_extent(last, 200, 100, 50, 25, 2.0);
    const double u = 50.0 / 200.0;
    const double v = 25.0 / 100.0;
    const double wx = 100.0 + u * 20.0;
    const double wy = 40.0 - v * 20.0;
    expect(std::fabs((next.minx + u * (next.maxx - next.minx)) - wx) < 1e-9,
           "cursor lon fixed");
    expect(std::fabs((next.maxy - v * (next.maxy - next.miny)) - wy) < 1e-9,
           "cursor lat fixed");
    expect(std::fabs((next.maxx - next.minx) - 10.0) < 1e-9, "lon span / 2");
    expect(std::fabs((next.maxy - next.miny) - 10.0) < 1e-9, "lat span / 2");
  }

  {
    const tool::BlitDestRect p = tool::pan_blit_dest(800, 600, 12, -8);
    expect(p.x == 12 && p.y == -8 && p.w == 800 && p.h == 600, "pan dest");
    const tool::WorldExtent last{0.0, 0.0, 80.0, 60.0};
    const tool::WorldExtent next =
        tool::pan_world_extent(last, 800, 600, 80, 60);
    expect(std::fabs(next.minx - (-8.0)) < 1e-9, "pan west");
    expect(std::fabs(next.maxy - 66.0) < 1e-9, "pan north on +screen-y");
  }

  {
    float yaw = 0.f;
    float pitch = 0.f;
    tool::orbit_from_drag(&yaw, &pitch, 10, -5, 0.01f);
    expect(yaw > 0.05f && pitch < 0.f, "orbit drag");
    float lo = -2.f;
    tool::orbit_from_drag(&yaw, &lo, 0, -1000, 0.01f);
    expect(lo >= -1.21f, "pitch clamp");
  }

  expect(tool::dolly_distance(4.f, 120, 1.f, 12.f) < 4.f, "dolly in");
  expect(tool::dolly_distance(4.f, -120, 1.f, 12.f) > 4.f, "dolly out");
  expect(tool::dolly_distance(1.2f, 12000, 1.2f, 12.f) >= 1.2f, "dolly min");

  expect(tool::hwheel_pan_dx(120) == 120, "hwheel notch");
  expect(tool::hwheel_pan_dx(-60) == -60, "hwheel smooth");
  expect(tool::hwheel_pan_dx(0) == 0, "hwheel zero");

  expect(tool::is_navigate_tool(nullptr), "null tool is navigate");
  expect(tool::is_navigate_tool("view.pan"), "view.pan");
  expect(tool::is_navigate_tool("view3d.trackball"), "view3d");
  expect(!tool::is_navigate_tool("draw.polygon"), "draw is exclusive");
  expect(!tool::is_navigate_tool("select.rect"), "select is exclusive");
  expect(!tool::is_navigate_tool("select.circle"), "circle select exclusive");

  {
    tool::PointerPinchTracker pinch;
    pinch.on_down(1, 0, 0);
    pinch.on_down(2, 100, 0);
    expect(pinch.contact_count() == 2, "two contacts");
    double scale = 0;
    expect(pinch.on_move(2, 120, 0, &scale), "pinch move");
    expect(scale > 1.0, "spread increases scale");
    pinch.on_up(1);
    expect(pinch.contact_count() == 1, "one left");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d camera_nav check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
