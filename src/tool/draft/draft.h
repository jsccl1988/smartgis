// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_DRAFT_H_
#define TOOL_DRAFT_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "tool/interaction/interaction.h"
#include "tool/tool_export.h"

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
// High bit: rubber-band zoom rect (view.zoom_in / nav). Not used by view.pan.
constexpr uint32_t kZoomRect = 0x02000000u;
// High bit: pointer/gesture end (mouse/touch up). Leftover browse settle uses
// this to urgent-redraw + sync present after pan drag.
constexpr uint32_t kGestureEnd = 0x04000000u;

inline constexpr uint32_t pack(uint32_t family, uint32_t code) {
  return (family << kFamilyShift) | (code & kCodeMask);
}
// High bits (touch pan / zoom rect / gesture end) share the uint32 with
// (family<<16)|code — strip them before reading the family field.
inline constexpr uint32_t family_of(uint32_t flags) {
  constexpr uint32_t kHigh =
      kTouchPan | kZoomRect | kGestureEnd;
  return (flags & ~kHigh) >> kFamilyShift;
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
inline constexpr bool is_zoom_rect(uint32_t flags) {
  return (flags & kZoomRect) != 0;
}
inline constexpr bool is_gesture_end(uint32_t flags) {
  return (flags & kGestureEnd) != 0;
}
}  // namespace draft_flags

using DraftCallback = std::function<void(const Draft&)>;

TOOL_EXPORT std::unique_ptr<Interaction> make_select_point(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_select_rect(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_select_circle(
    DraftCallback on_complete);
TOOL_EXPORT std::unique_ptr<Interaction> make_select_polygon(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_draw_point(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_draw_linestring(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_draw_polygon(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_draw_rect(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_view_zoom_in(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_view_zoom_out(
    DraftCallback on_complete, uint32_t default_flags = 0);
TOOL_EXPORT std::unique_ptr<Interaction> make_view_pan(
    DraftCallback on_complete, uint32_t default_flags = 0);
// Click a vertex of the already-selected feature. The shell resolves the
// real FeatureId; this interaction only emits the pixel draft.
TOOL_EXPORT std::unique_ptr<Interaction> make_edit_vertex(
    DraftCallback on_complete);
TOOL_EXPORT std::unique_ptr<Interaction> make_view3d_trackball(
    DraftCallback on_complete);
TOOL_EXPORT std::unique_ptr<Interaction> make_view3d_sphere(
    DraftCallback on_complete);
TOOL_EXPORT std::unique_ptr<Interaction> make_view3d_fps(
    DraftCallback on_complete);

}  // namespace tool

#endif  // TOOL_DRAFT_H_
