// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/view_frame.h"

#include <cstdio>

#include "content/browser/document/gis_scene.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int run_view_frame_tests() {
  {
    content::ViewFrame frame;
    int vx = 0;
    int vy = 0;
    frame.map_to_view(0.0, 0.0, &vx, &vy);
    expect(vx == 0 && vy == 0, "origin before pan");
    frame.apply_pan(12, -4);
    int vx2 = 0;
    int vy2 = 0;
    frame.map_to_view(0.0, 0.0, &vx2, &vy2);
    expect(vx2 == vx + 12 && vy2 == vy - 4, "apply_pan changes map_to_view");
  }

  {
    // GisScene() does not seed vertices. fit_extent must leave the frame.
    content::GisScene scene;
    content::ViewFrame frame;
    frame.apply_pan(3, -8);
    const double pan_x = frame.pan_x();
    const double pan_y = frame.pan_y();
    const double scale = frame.scale();
    frame.fit_extent(scene, 640, 480);
    expect(frame.pan_x() == pan_x, "empty fit keeps pan_x");
    expect(frame.pan_y() == pan_y, "empty fit keeps pan_y");
    expect(frame.scale() == scale, "empty fit keeps scale");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d view_frame_test fail(s)\n", g_fails);
  }
  return g_fails;
}
