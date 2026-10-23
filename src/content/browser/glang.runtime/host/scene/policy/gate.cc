// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/scene/policy/gate.h"

#include <cstdlib>
#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace content {
namespace detail {
namespace {

bool apply_rc(content::CapabilityHost& host, int rc) {
  if (rc != 0) {
    host.fail_rc = rc;
    return false;
  }
  return true;
}

void detach_views(content::CapabilityHost& host) {
  if (host.view.detach_views) {
    host.view.detach_views();
  }
}

bool read_view_load(content::CapabilityHost& host,
                    std::string_view face,
                    int timeout_ms,
                    content::ViewLoadStatus* out) {
  if (!out || !host.view.load_status ||
      !host.view.load_status(std::string(face), timeout_ms, out)) {
    return false;
  }
  if (host.view.on_app_event) {
    host.view.on_app_event("map.load", *out);
  }
  return true;
}

// Product faces that share the scene gate codes. Other faces use the map codes.
bool is_scene_face(std::string_view face) {
  return face == "scene" || face == "scene3d";
}

}  // namespace

bool wait_viewport(content::CapabilityHost& host,
                   std::string_view face,
                   int timeout_ms,
                   int want_frame) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, face, timeout_ms, &status)) {
    return false;
  }
  const bool scene = is_scene_face(face);
  const int pane_rc = scene ? 9 : 3;
  const int frame_rc = scene ? 10 : 3;
  if (!status.pane_present) {
    return apply_rc(host, pane_rc);
  }
  if (!status.content_view) {
    return true;
  }
  if (!status.hwnd_alive) {
    return apply_rc(host, pane_rc);
  }
  if (want_frame && !status.frame_ready) {
    return apply_rc(host, frame_rc);
  }
  return true;
}

bool expect_scene_visible(content::CapabilityHost& host) {
  content::ViewLoadStatus scene;
  content::ViewLoadStatus map;
  if (!read_view_load(host, "scene", 0, &scene) ||
      !read_view_load(host, "map", 0, &map)) {
    return false;
  }
  if (!scene.hwnd_alive) {
    return apply_rc(host, 9);
  }
  if (map.hwnd_alive && map.visible) {
    return apply_rc(host, 36);
  }
  if (!scene.visible) {
    return apply_rc(host, 37);
  }
  return true;
}

bool expect_orbit_moved(content::CapabilityHost& host) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, "scene", 0, &status)) {
    return false;
  }
  if (!status.orbit_moved) {
    return apply_rc(host, 25);
  }
  return true;
}

bool expect_map_hwnd_sync(content::CapabilityHost& host) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, "map", 0, &status)) {
    return false;
  }
  const auto fail = [&](int rc) {
detach_views(host);
    return apply_rc(host, rc);
  };
  if (status.view_w <= 0 || status.view_h <= 0) {
    return fail(31);
  }
  if (!status.hwnd_alive) {
    return fail(35);
  }
  constexpr int kTol = 2;
  if (std::abs(status.hwnd_x - status.view_x) > kTol ||
      std::abs(status.hwnd_y - status.view_y) > kTol) {
    return fail(33);
  }
  if (std::abs(status.hwnd_w - status.view_w) > kTol ||
      std::abs(status.hwnd_h - status.view_h) > kTol) {
    return fail(34);
  }
  if (status.menu_min_px > 0 && status.menu_h < status.menu_min_px) {
    return fail(32);
  }
  return true;
}

}  // namespace detail
}  // namespace content
