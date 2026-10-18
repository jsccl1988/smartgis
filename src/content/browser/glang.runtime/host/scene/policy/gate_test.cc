// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/scene/policy/gate.h"

#include "gtest/gtest.h"

#include <string>

namespace {

content::CapabilityHost host_with(
    content::ViewLoadStatus scene,
    content::ViewLoadStatus map) {
  content::CapabilityHost host;
  host.view.load_status = [scene, map](const std::string& face, int,
                                       content::ViewLoadStatus* out) {
    if (!out) {
      return false;
    }
    if (face == "scene" || face == "scene3d") {
      *out = scene;
    } else {
      *out = map;
    }
    out->face = face;
    return true;
  };
  return host;
}

content::ViewLoadStatus pane_only() {
  content::ViewLoadStatus status;
  status.pane_present = 1;
  status.content_view = 1;
  status.hwnd_alive = 1;
  status.frame_ready = 1;
  status.visible = 1;
  status.view_w = 100;
  status.view_h = 80;
  status.hwnd_w = 100;
  status.hwnd_h = 80;
  return status;
}

TEST(SceneGate, SceneMissUsesSceneCodes) {
  content::ViewLoadStatus empty;
  content::CapabilityHost host = host_with(empty, empty);
  EXPECT_FALSE(content::detail::wait_viewport(host, "scene3d", 0, 1));
  EXPECT_EQ(host.fail_rc, 9);

  host.fail_rc = 0;
  content::ViewLoadStatus pane = pane_only();
  pane.frame_ready = 0;
  host = host_with(pane, pane);
  EXPECT_FALSE(content::detail::wait_viewport(host, "scene", 0, 1));
  EXPECT_EQ(host.fail_rc, 10);
}

TEST(SceneGate, MapMissUsesMapCode) {
  content::ViewLoadStatus empty;
  content::CapabilityHost host = host_with(empty, empty);
  EXPECT_FALSE(content::detail::wait_viewport(host, "map", 0, 1));
  EXPECT_EQ(host.fail_rc, 3);

  host.fail_rc = 0;
  content::ViewLoadStatus pane = pane_only();
  pane.frame_ready = 0;
  host = host_with(pane, pane);
  EXPECT_FALSE(content::detail::wait_viewport(host, "map", 0, 1));
  EXPECT_EQ(host.fail_rc, 3);
}

TEST(SceneGate, VisibleOrbitAndHwnd) {
  content::ViewLoadStatus scene = pane_only();
  scene.hwnd_alive = 0;
  scene.visible = 0;
  content::ViewLoadStatus map = pane_only();
  map.visible = 1;
  content::CapabilityHost host = host_with(scene, map);
  EXPECT_FALSE(content::detail::expect_scene_visible(host));
  EXPECT_EQ(host.fail_rc, 9);

  scene.hwnd_alive = 1;
  scene.visible = 0;
  host = host_with(scene, map);
  host.fail_rc = 0;
  EXPECT_FALSE(content::detail::expect_scene_visible(host));
  EXPECT_EQ(host.fail_rc, 36);

  map.visible = 0;
  scene.visible = 0;
  host = host_with(scene, map);
  EXPECT_FALSE(content::detail::expect_scene_visible(host));
  EXPECT_EQ(host.fail_rc, 37);

  scene.visible = 1;
  scene.orbit_moved = 0;
  host = host_with(scene, map);
  EXPECT_TRUE(content::detail::expect_scene_visible(host));
  EXPECT_FALSE(content::detail::expect_orbit_moved(host));
  EXPECT_EQ(host.fail_rc, 25);

  scene.orbit_moved = 1;
  host = host_with(scene, map);
  host.fail_rc = 0;
  EXPECT_TRUE(content::detail::expect_orbit_moved(host));

  content::ViewLoadStatus bad = pane_only();
  bad.view_w = 0;
  int detached = 0;
  host = host_with(scene, bad);
  host.view.detach_views = [&]() { ++detached; };
  EXPECT_FALSE(content::detail::expect_map_hwnd_sync(host));
  EXPECT_EQ(host.fail_rc, 31);
  EXPECT_EQ(detached, 1);
}

}  // namespace
