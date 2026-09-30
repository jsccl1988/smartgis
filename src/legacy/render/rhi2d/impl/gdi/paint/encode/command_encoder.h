// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_

#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_buffer.h"
#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"

namespace render {
namespace detail {

// Records a GDI pass into a GdiCommandBuffer (clear / fill / stroke / blit /
// clip / pen / brush / polyline / polygon / text). Replay is separate:
// replay(buffer, surface) via CreateCompatibleDC + GDI draws.
class GdiCommandEncoder {
 public:
  GdiCommandEncoder() = default;

  GdiCommandEncoder(const GdiCommandEncoder&) = delete;
  GdiCommandEncoder& operator=(const GdiCommandEncoder&) = delete;

  // Starts recording against |target| (non-owning; used as pass context).
  // Returns false when |target| is null or already in a pass.
  bool begin_pass(GdiSurface* target);

  // Appends EndPass and closes the recording pass.
  // Returns false when no pass is open.
  bool end_pass();

  // Records a full-surface clear. Returns false outside a pass.
  bool clear(COLORREF color);

  // Records an axis-aligned fill. Returns false outside a pass or for w/h < 1.
  bool fill_rect(int x, int y, int w, int h, COLORREF color);

  // Records an axis-aligned stroke (outline). |pen_width| < 1 is treated as 1.
  bool stroke_rect(int x, int y, int w, int h, COLORREF color, int pen_width);

  // Records an intersecting clip rect for subsequent draws in this pass.
  bool clip_rect(int x, int y, int w, int h);

  // Clears the clip region for subsequent draws.
  bool reset_clip();

  // Records BitBlt from |src| into the target. |src| must stay alive until
  // replay.
  bool blit(HBITMAP src, int dest_x, int dest_y, int dest_w, int dest_h,
            int src_x, int src_y, int src_w, int src_h);

  // Records a style stub (pen color); replay may use it as fallback stroke
  // color.
  bool set_style(uint32_t style_id, COLORREF color);

  // Records current pen for subsequent polyline/ellipse strokes.
  bool set_pen(COLORREF color, int width, int style = PS_SOLID);

  // Records current brush for subsequent polygon/ellipse fills.
  bool set_brush(COLORREF color, int style = BS_SOLID, int hatch = HS_HORIZONTAL);

  // Records a device-space polyline (copies POINT[] into the blob).
  bool polyline(const POINT* pts, int count);

  // Records PolyPolygon (copies points + ring counts into the blob).
  bool poly_polygon(const POINT* pts, const int* ring_counts, int n_rings);

  // Records PolyPolyline (copies points + segment counts into the blob).
  bool poly_polyline(const POINT* pts, const int* poly_counts, int n_polys);

  // Records an ellipse in device pixels.
  bool ellipse(int left, int top, int right, int bottom);

  // Records device-space text (|text| copied into blob; |n| bytes, no NUL req).
  bool text(int x, int y, const char* text, int n, int height_px, int halo_px,
            float angle_deg);

  // Moves out the recorded buffer and resets encoder state.
  GdiCommandBuffer take_buffer();

  bool is_recording() const { return recording_; }
  GdiSurface* target() const { return target_; }

  // Append into the open buffer blob (for callers that build custom ranges).
  GdiBlobRange append_blob(const void* data, size_t n);

 private:
  // Pushes |cmd| when a pass is open; shared by all record_* helpers.
  bool push_cmd(GdiCommand cmd);

  GdiSurface* target_ = nullptr;
  bool recording_ = false;
  GdiCommandBuffer buffer_;
};

// Plays |buffer| onto |target| with a memory DC.
// Bumps generation and marks dirty on pixel writes. Returns false on GDI setup
// fail.
bool replay(const GdiCommandBuffer& buffer, GdiSurface& target);

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_
