// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_GESTURES_H_
#define TOOL_GESTURES_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "tool/interaction.h"

// Pixel-space drafts produced by select / append interactions.
namespace tool {

enum class DraftKind { kPoint, kRect, kLineString, kPolygon, kKey, kWheel, kPick };

struct DraftPoint {
  int32_t x_px = 0;
  int32_t y_px = 0;
};

// Pixel-space stroke, leftover 3D key/wheel, or click-to-pick. No camera types.
struct Draft {
  DraftKind kind = DraftKind::kPoint;
  std::vector<DraftPoint> points;
  uint32_t key = 0;
  int32_t wheel = 0;
};

using DraftCallback = std::function<void(const Draft&)>;

std::unique_ptr<Interaction> make_select_point(DraftCallback on_complete);
std::unique_ptr<Interaction> make_select_rect(DraftCallback on_complete);
std::unique_ptr<Interaction> make_select_polygon(DraftCallback on_complete);
std::unique_ptr<Interaction> make_draw_point(DraftCallback on_complete);
std::unique_ptr<Interaction> make_draw_linestring(DraftCallback on_complete);
std::unique_ptr<Interaction> make_draw_polygon(DraftCallback on_complete);
std::unique_ptr<Interaction> make_draw_rect(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view_zoom_in(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view_zoom_out(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view_pan(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view3d_trackball(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view3d_sphere(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view3d_fps(DraftCallback on_complete);

}  // namespace tool

#endif  // TOOL_GESTURES_H_
