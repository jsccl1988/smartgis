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
  // Fine subtype: (family << 16) | code. See draft_flags and SP1b §9.2.
  uint32_t flags = 0;
};

// Coarse command/interaction ids stay few; fine digitize/select subtype lives here.
namespace draft_flags {
constexpr uint32_t kFamilyShift = 16;
constexpr uint32_t kCodeMask = 0xffffu;
constexpr uint32_t kFamilyNone = 0;
constexpr uint32_t kFamilyPoint = 1;
constexpr uint32_t kFamilyLine = 2;
constexpr uint32_t kFamilyRegion = 3;
constexpr uint32_t kFamilySelect = 4;
constexpr uint32_t kSelectCircleCode = 1;
// High bit outside Win32 MK_* and (family<<16)|code packing: two-finger pan.
constexpr uint32_t kTouchPan = 0x01000000u;

inline constexpr uint32_t pack(uint32_t family, uint32_t code) {
  return (family << kFamilyShift) | (code & kCodeMask);
}
inline constexpr uint32_t family_of(uint32_t flags) {
  return flags >> kFamilyShift;
}
inline constexpr uint32_t code_of(uint32_t flags) {
  return flags & kCodeMask;
}
inline constexpr bool is_select_circle(uint32_t flags) {
  return family_of(flags) == kFamilySelect &&
         code_of(flags) == kSelectCircleCode;
}
inline constexpr bool is_touch_pan(uint32_t flags) {
  return (flags & kTouchPan) != 0;
}
}  // namespace draft_flags

using DraftCallback = std::function<void(const Draft&)>;

std::unique_ptr<Interaction> make_select_point(DraftCallback on_complete,
                                               uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_select_rect(DraftCallback on_complete,
                                              uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_select_circle(DraftCallback on_complete);
std::unique_ptr<Interaction> make_select_polygon(DraftCallback on_complete,
                                                 uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_draw_point(DraftCallback on_complete,
                                             uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_draw_linestring(DraftCallback on_complete,
                                                  uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_draw_polygon(DraftCallback on_complete,
                                               uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_draw_rect(DraftCallback on_complete,
                                            uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_view_zoom_in(DraftCallback on_complete,
                                               uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_view_zoom_out(DraftCallback on_complete,
                                                uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_view_pan(DraftCallback on_complete,
                                           uint32_t default_flags = 0);
std::unique_ptr<Interaction> make_view3d_trackball(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view3d_sphere(DraftCallback on_complete);
std::unique_ptr<Interaction> make_view3d_fps(DraftCallback on_complete);

}  // namespace tool

#endif  // TOOL_GESTURES_H_
