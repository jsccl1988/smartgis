// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_HUD_HUD_IR_H_
#define VISTA_PASS_WORLD_HUD_HUD_IR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"

namespace vista {

// Screen-space HUD items for World present (kOverlay). Content fills this;
// HudPass rasters + uploads. No HWND / HDC product SoT.
struct VISTA_EXPORT HudLabel {
  std::string text;  // UTF-8
  float x = 0.f;
  float y = 0.f;
  uint8_t rgba[4] = {255, 255, 255, 255};
  bool halo = false;
};

struct VISTA_EXPORT HudLine {
  float x0 = 0.f;
  float y0 = 0.f;
  float x1 = 0.f;
  float y1 = 0.f;
  uint8_t rgba[4] = {120, 200, 255, 255};
};

struct VISTA_EXPORT HudCompass {
  float cx = 0.f;
  float cy = 0.f;
  float radius = 28.f;
  float north_angle_rad = 0.f;  // screen: 0 = up
  bool enabled = false;
};

struct VISTA_EXPORT HudBadge {
  std::string text;  // UTF-8 engine / fps label
  bool enabled = false;
};

// One frame of scene3d chrome overlay (compass, status, place-names, wind, badge).
struct VISTA_EXPORT HudFrame {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  uint64_t generation = 0;
  std::vector<HudLabel> labels;
  std::vector<HudLine> lines;
  HudCompass compass;
  HudBadge badge;

  void clear() {
    width_px = 0;
    height_px = 0;
    generation = 0;
    labels.clear();
    lines.clear();
    compass = {};
    badge = {};
  }

  bool empty() const {
    return labels.empty() && lines.empty() && !compass.enabled &&
           !badge.enabled;
  }
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_HUD_HUD_IR_H_
