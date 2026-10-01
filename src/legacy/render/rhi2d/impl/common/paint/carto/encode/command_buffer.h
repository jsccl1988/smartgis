// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_BUFFER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_BUFFER_H_

#include <cstdint>
#include <cstring>
#include <vector>

#include "legacy/core/macros/macros.h"

namespace render {

// Op codes for GDI command buffer (clear / fill / stroke / blit / clip /
// style / device-space geom).
enum class Rhi2dCommandOp : uint8_t {
  kClear = 0,
  kFillRect = 1,
  kBlit = 2,
  kSetStyle = 3,
  kEndPass = 4,
  kStrokeRect = 5,
  kClipRect = 6,
  kResetClip = 7,
  kSetPen = 8,
  kSetBrush = 9,
  kPolyline = 10,
  kPolyPolygon = 11,
  kEllipse = 12,
  kText = 13,
  kPolyPolyline = 14,
};

// POD args for Rhi2dCommandOp::kClear.
struct Rhi2dClearArgs {
  COLORREF color = 0;
};

// POD args for Rhi2dCommandOp::kFillRect.
struct Rhi2dFillRectArgs {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  COLORREF color = 0;
};

// POD args for Rhi2dCommandOp::kBlit (src_bitmap as uintptr_t for SelectObject).
struct Rhi2dBlitArgs {
  uintptr_t src_bitmap = 0;
  int dest_x = 0;
  int dest_y = 0;
  int dest_w = 0;
  int dest_h = 0;
  int src_x = 0;
  int src_y = 0;
  int src_w = 0;
  int src_h = 0;
};

// POD stub / optional pen color for stroke (execute may ignore style_id).
struct Rhi2dSetStyleArgs {
  uint32_t style_id = 0;
  COLORREF color = 0;
};

// POD args for Rhi2dCommandOp::kStrokeRect.
struct Rhi2dStrokeRectArgs {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  COLORREF color = 0;
  int width = 1;
};

// POD args for Rhi2dCommandOp::kClipRect.
struct Rhi2dClipRectArgs {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// Blob byte range inside Rhi2dCommandBuffer::blob().
struct Rhi2dBlobRange {
  uint32_t offset = 0;
  uint32_t size = 0;
};

// Current pen for subsequent geom ops.
struct Rhi2dSetPenArgs {
  COLORREF color = 0;
  int width = 1;
  int style = PS_SOLID;
};

// Current brush for subsequent geom ops.
struct Rhi2dSetBrushArgs {
  COLORREF color = 0;
  int style = BS_SOLID;
  int hatch = HS_HORIZONTAL;
};

// Device-space polyline (POINT[] in blob).
struct GdiPolylineArgs {
  Rhi2dBlobRange points;
  uint32_t count = 0;
};

// Device-space PolyPolygon (POINT[] + int ring counts in blob).
struct GdiPolyPolygonArgs {
  Rhi2dBlobRange points;
  Rhi2dBlobRange counts;
  uint32_t n_rings = 0;
};

// Device-space PolyPolyline (POINT[] + int segment counts in blob).
struct GdiPolyPolylineArgs {
  Rhi2dBlobRange points;
  Rhi2dBlobRange counts;
  uint32_t n_polys = 0;
};

// Axis-aligned ellipse in device pixels.
struct GdiEllipseArgs {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

// Device-space text (UTF-8 / ANSI bytes in blob, size includes no NUL require).
struct GdiTextArgs {
  Rhi2dBlobRange text;
  int x = 0;
  int y = 0;
  int height_px = 12;
  int halo_px = 0;
  float angle_deg = 0.f;
};

// One recorded command: op + POD payload union.
struct Rhi2dCommand {
  Rhi2dCommandOp op = Rhi2dCommandOp::kEndPass;
  union {
    Rhi2dClearArgs clear;
    Rhi2dFillRectArgs fill_rect;
    Rhi2dBlitArgs blit;
    Rhi2dSetStyleArgs set_style;
    Rhi2dStrokeRectArgs stroke_rect;
    Rhi2dClipRectArgs clip_rect;
    Rhi2dSetPenArgs set_pen;
    Rhi2dSetBrushArgs set_brush;
    GdiPolylineArgs polyline;
    GdiPolyPolygonArgs poly_polygon;
    GdiPolyPolylineArgs poly_polyline;
    GdiEllipseArgs ellipse;
    GdiTextArgs text;
  };

  Rhi2dCommand() : op(Rhi2dCommandOp::kEndPass), clear{} {}
};

// Owns a sequence of typed GDI draw ops plus a side blob for points/text.
class Rhi2dCommandBuffer {
 public:
  Rhi2dCommandBuffer() = default;

  Rhi2dCommandBuffer(Rhi2dCommandBuffer&&) = default;
  Rhi2dCommandBuffer& operator=(Rhi2dCommandBuffer&&) = default;

  Rhi2dCommandBuffer(const Rhi2dCommandBuffer&) = delete;
  Rhi2dCommandBuffer& operator=(const Rhi2dCommandBuffer&) = delete;

  void push(Rhi2dCommand cmd) { ops_.push_back(cmd); }

  void clear() {
    ops_.clear();
    blob_.clear();
  }

  bool empty() const { return ops_.empty(); }
  size_t size() const { return ops_.size(); }

  const std::vector<Rhi2dCommand>& ops() const { return ops_; }
  std::vector<Rhi2dCommand>& ops() { return ops_; }

  const std::vector<uint8_t>& blob() const { return blob_; }

  // Appends |n| bytes; returns range. Empty when n==0.
  Rhi2dBlobRange append_blob(const void* data, size_t n) {
    Rhi2dBlobRange range;
    if (!data || n == 0) {
      return range;
    }
    range.offset = static_cast<uint32_t>(blob_.size());
    range.size = static_cast<uint32_t>(n);
    const auto* bytes = static_cast<const uint8_t*>(data);
    blob_.insert(blob_.end(), bytes, bytes + n);
    return range;
  }

  // Pointer into blob for |range|, or nullptr when OOB.
  const uint8_t* blob_at(Rhi2dBlobRange range) const {
    if (range.size == 0) {
      return nullptr;
    }
    const size_t end =
        static_cast<size_t>(range.offset) + static_cast<size_t>(range.size);
    if (end > blob_.size()) {
      return nullptr;
    }
    return blob_.data() + range.offset;
  }

 private:
  std::vector<Rhi2dCommand> ops_;
  std::vector<uint8_t> blob_;
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_BUFFER_H_
