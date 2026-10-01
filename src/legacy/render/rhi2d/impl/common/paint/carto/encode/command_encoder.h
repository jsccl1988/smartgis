// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_

#include "legacy/render/rhi2d/impl/common/paint/carto/encode/command_buffer.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/owned.h"

namespace render {
namespace detail {

// Records a GDI pass into a Rhi2dCommandBuffer (clear / fill / stroke / blit /
// clip / pen / brush / polyline / polygon / text). Replay is separate:
// execute(buffer, surface) via CreateCompatibleDC + GDI draws.
class Rhi2dCommandEncoder {
 public:
  Rhi2dCommandEncoder() = default;

  Rhi2dCommandEncoder(const Rhi2dCommandEncoder&) = delete;
  Rhi2dCommandEncoder& operator=(const Rhi2dCommandEncoder&) = delete;

  // Starts recording against |target| (non-owning; used as pass context).
  // Returns false when |target| is null or already in a pass.
  bool begin_pass(Rhi2dSurface* target);

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
  // execute.
  bool blit(HBITMAP src, int dest_x, int dest_y, int dest_w, int dest_h,
            int src_x, int src_y, int src_w, int src_h);

  // Records a style stub (pen color); execute may use it as fallback stroke
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
  Rhi2dCommandBuffer take_buffer();

  bool is_recording() const { return recording_; }
  Rhi2dSurface* target() const { return target_; }

  // Append into the open buffer blob (for callers that build custom ranges).
  Rhi2dBlobRange append_blob(const void* data, size_t n);

 private:
  // Pushes |cmd| when a pass is open; shared by all record_* helpers.
  bool push_cmd(Rhi2dCommand cmd);

  void clear_style_cache();

  Rhi2dSurface* target_ = nullptr;
  bool recording_ = false;
  Rhi2dCommandBuffer buffer_;

  // Skip redundant SetPen/SetBrush ops within one pass (execute CreatePen cost).
  bool last_pen_valid_ = false;
  COLORREF last_pen_color_ = 0;
  int last_pen_width_ = 0;
  int last_pen_style_ = 0;
  bool last_brush_valid_ = false;
  COLORREF last_brush_color_ = 0;
  int last_brush_style_ = 0;
  int last_brush_hatch_ = 0;
};

// Plays |buffer| onto |target| with a memory DC.
// Bumps generation and marks dirty on pixel writes. Returns false on GDI setup
// fail.
bool execute(const Rhi2dCommandBuffer& buffer, Rhi2dSurface& target);

// Execute full-viewport commands into a tile-sized |target|. Window origin is
// shifted to (|origin_x|, |origin_y|) so absolute device coords from the
// full-frame encode land on the tile DIB. kClear fills the whole tile.
bool execute_tile(const Rhi2dCommandBuffer& buffer, Rhi2dSurface& target,
                  int origin_x, int origin_y);

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_CORE_ENCODE_COMMAND_ENCODER_H_
