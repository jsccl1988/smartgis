// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"

#include <algorithm>
#include <utility>

#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"

namespace scenic {
namespace detail {
namespace {

void note_write(Rhi2dSurface& target, int x, int y, int w, int h) {
  target.bump_generation();
  target.mark_dirty(x, y, w, h);
}

// Logical-space tile rect [ox, ox+tw) x [oy, oy+th) (viewport coords).
bool rect_hits_tile(int x0, int y0, int x1, int y1, int ox, int oy, int tw,
                    int th) {
  return x0 < ox + tw && x1 > ox && y0 < oy + th && y1 > oy;
}

bool points_hit_tile(const POINT* pts, int n, int ox, int oy, int tw, int th) {
  if (!pts || n < 1) {
    return false;
  }
  LONG min_x = pts[0].x;
  LONG max_x = pts[0].x;
  LONG min_y = pts[0].y;
  LONG max_y = pts[0].y;
  for (int i = 1; i < n; ++i) {
    min_x = (std::min)(min_x, pts[i].x);
    max_x = (std::max)(max_x, pts[i].x);
    min_y = (std::min)(min_y, pts[i].y);
    max_y = (std::max)(max_y, pts[i].y);
  }
  return rect_hits_tile(static_cast<int>(min_x), static_cast<int>(min_y),
                        static_cast<int>(max_x), static_cast<int>(max_y), ox,
                        oy, tw, th);
}

int total_point_count(const int* counts, int n) {
  int sum = 0;
  if (!counts || n < 1) {
    return 0;
  }
  for (int i = 0; i < n; ++i) {
    sum += counts[i];
  }
  return sum;
}

}  // namespace

void Rhi2dCommandEncoder::clear_style_cache() {
  last_pen_valid_ = false;
  last_brush_valid_ = false;
}

bool Rhi2dCommandEncoder::begin_pass(Rhi2dSurface* target) {
  if (!target || recording_) {
    return false;
  }
  target_ = target;
  recording_ = true;
  buffer_.clear();
  clear_style_cache();
  return true;
}

bool Rhi2dCommandEncoder::end_pass() {
  if (!recording_) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kEndPass;
  buffer_.push(cmd);
  recording_ = false;
  target_ = nullptr;
  clear_style_cache();
  return true;
}

bool Rhi2dCommandEncoder::push_cmd(Rhi2dCommand cmd) {
  if (!recording_) {
    return false;
  }
  buffer_.push(cmd);
  return true;
}

Rhi2dBlobRange Rhi2dCommandEncoder::append_blob(const void* data, size_t n) {
  if (!recording_) {
    return {};
  }
  return buffer_.append_blob(data, n);
}

bool Rhi2dCommandEncoder::clear(COLORREF color) {
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kClear;
  cmd.clear.color = color;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::fill_rect(int x, int y, int w, int h, COLORREF color) {
  if (w < 1 || h < 1) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kFillRect;
  cmd.fill_rect.x = x;
  cmd.fill_rect.y = y;
  cmd.fill_rect.w = w;
  cmd.fill_rect.h = h;
  cmd.fill_rect.color = color;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::stroke_rect(int x, int y, int w, int h, COLORREF color,
                                    int pen_width) {
  if (w < 1 || h < 1) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kStrokeRect;
  cmd.stroke_rect.x = x;
  cmd.stroke_rect.y = y;
  cmd.stroke_rect.w = w;
  cmd.stroke_rect.h = h;
  cmd.stroke_rect.color = color;
  cmd.stroke_rect.width = pen_width < 1 ? 1 : pen_width;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::clip_rect(int x, int y, int w, int h) {
  if (w < 1 || h < 1) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kClipRect;
  cmd.clip_rect.x = x;
  cmd.clip_rect.y = y;
  cmd.clip_rect.w = w;
  cmd.clip_rect.h = h;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::reset_clip() {
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kResetClip;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
                             int dest_h, int src_x, int src_y, int src_w,
                             int src_h) {
  if (!src || dest_w < 1 || dest_h < 1 || src_w < 1 || src_h < 1) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kBlit;
  cmd.blit.src_bitmap = reinterpret_cast<uintptr_t>(src);
  cmd.blit.dest_x = dest_x;
  cmd.blit.dest_y = dest_y;
  cmd.blit.dest_w = dest_w;
  cmd.blit.dest_h = dest_h;
  cmd.blit.src_x = src_x;
  cmd.blit.src_y = src_y;
  cmd.blit.src_w = src_w;
  cmd.blit.src_h = src_h;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::set_style(uint32_t style_id, COLORREF color) {
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kSetStyle;
  cmd.set_style.style_id = style_id;
  cmd.set_style.color = color;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::set_pen(COLORREF color, int width, int style) {
  const int w = width < 1 ? 1 : width;
  if (last_pen_valid_ && last_pen_color_ == color && last_pen_width_ == w &&
      last_pen_style_ == style) {
    return recording_;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kSetPen;
  cmd.set_pen.color = color;
  cmd.set_pen.width = w;
  cmd.set_pen.style = style;
  if (!push_cmd(cmd)) {
    return false;
  }
  last_pen_valid_ = true;
  last_pen_color_ = color;
  last_pen_width_ = w;
  last_pen_style_ = style;
  return true;
}

bool Rhi2dCommandEncoder::set_brush(COLORREF color, int style, int hatch) {
  if (last_brush_valid_ && last_brush_color_ == color &&
      last_brush_style_ == style && last_brush_hatch_ == hatch) {
    return recording_;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kSetBrush;
  cmd.set_brush.color = color;
  cmd.set_brush.style = style;
  cmd.set_brush.hatch = hatch;
  if (!push_cmd(cmd)) {
    return false;
  }
  last_brush_valid_ = true;
  last_brush_color_ = color;
  last_brush_style_ = style;
  last_brush_hatch_ = hatch;
  return true;
}

bool Rhi2dCommandEncoder::polyline(const POINT* pts, int count) {
  if (!pts || count < 2) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kPolyline;
  cmd.polyline.count = static_cast<uint32_t>(count);
  cmd.polyline.points =
      append_blob(pts, static_cast<size_t>(count) * sizeof(POINT));
  if (cmd.polyline.points.size == 0) {
    return false;
  }
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::poly_polygon(const POINT* pts, const int* ring_counts,
                                     int n_rings) {
  if (!pts || !ring_counts || n_rings < 1) {
    return false;
  }
  int total = 0;
  for (int i = 0; i < n_rings; ++i) {
    if (ring_counts[i] < 1) {
      return false;
    }
    total += ring_counts[i];
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kPolyPolygon;
  cmd.poly_polygon.n_rings = static_cast<uint32_t>(n_rings);
  cmd.poly_polygon.points =
      append_blob(pts, static_cast<size_t>(total) * sizeof(POINT));
  cmd.poly_polygon.counts =
      append_blob(ring_counts, static_cast<size_t>(n_rings) * sizeof(int));
  if (cmd.poly_polygon.points.size == 0 || cmd.poly_polygon.counts.size == 0) {
    return false;
  }
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::poly_polyline(const POINT* pts, const int* poly_counts,
                                      int n_polys) {
  if (!pts || !poly_counts || n_polys < 1) {
    return false;
  }
  int total = 0;
  for (int i = 0; i < n_polys; ++i) {
    if (poly_counts[i] < 2) {
      return false;
    }
    total += poly_counts[i];
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kPolyPolyline;
  cmd.poly_polyline.n_polys = static_cast<uint32_t>(n_polys);
  cmd.poly_polyline.points =
      append_blob(pts, static_cast<size_t>(total) * sizeof(POINT));
  cmd.poly_polyline.counts =
      append_blob(poly_counts, static_cast<size_t>(n_polys) * sizeof(int));
  if (cmd.poly_polyline.points.size == 0 ||
      cmd.poly_polyline.counts.size == 0) {
    return false;
  }
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::ellipse(int left, int top, int right, int bottom) {
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kEllipse;
  cmd.ellipse.left = left;
  cmd.ellipse.top = top;
  cmd.ellipse.right = right;
  cmd.ellipse.bottom = bottom;
  return push_cmd(cmd);
}

bool Rhi2dCommandEncoder::text(int x, int y, const char* text, int n,
                             int height_px, int halo_px, float angle_deg) {
  if (!text || n < 1) {
    return false;
  }
  Rhi2dCommand cmd;
  cmd.op = Rhi2dCommandOp::kText;
  cmd.text.x = x;
  cmd.text.y = y;
  cmd.text.height_px = height_px;
  cmd.text.halo_px = halo_px;
  cmd.text.angle_deg = angle_deg;
  cmd.text.text = append_blob(text, static_cast<size_t>(n));
  if (cmd.text.text.size == 0) {
    return false;
  }
  return push_cmd(cmd);
}

Rhi2dCommandBuffer Rhi2dCommandEncoder::take_buffer() {
  recording_ = false;
  target_ = nullptr;
  clear_style_cache();
  return std::move(buffer_);
}

bool execute(const Rhi2dCommandBuffer& buffer, Rhi2dSurface& target) {
  if (!target.bitmap || target.width < 1 || target.height < 1) {
    return false;
  }

  HDC hdc = ::CreateCompatibleDC(nullptr);
  if (!hdc) {
    return false;
  }
  HBITMAP old = static_cast<HBITMAP>(::SelectObject(hdc, target.bitmap));
  if (!old) {
    ::DeleteDC(hdc);
    return false;
  }

  ScopedPaintBackend backend(hdc);
  [[maybe_unused]] COLORREF style_color = RGB(0, 0, 0);

  for (const Rhi2dCommand& cmd : buffer.ops()) {
    switch (cmd.op) {
      case Rhi2dCommandOp::kClear:
        backend->clear_rect(0, 0, target.width, target.height, cmd.clear.color);
        note_write(target, 0, 0, target.width, target.height);
        break;
      case Rhi2dCommandOp::kFillRect:
        backend->fill_rect(cmd.fill_rect.x, cmd.fill_rect.y, cmd.fill_rect.w,
                         cmd.fill_rect.h, cmd.fill_rect.color);
        note_write(target, cmd.fill_rect.x, cmd.fill_rect.y, cmd.fill_rect.w,
                   cmd.fill_rect.h);
        break;
      case Rhi2dCommandOp::kStrokeRect:
        backend->stroke_rect(cmd.stroke_rect.x, cmd.stroke_rect.y,
                           cmd.stroke_rect.w, cmd.stroke_rect.h,
                           cmd.stroke_rect.color, cmd.stroke_rect.width);
        note_write(target, cmd.stroke_rect.x, cmd.stroke_rect.y,
                   cmd.stroke_rect.w, cmd.stroke_rect.h);
        break;
      case Rhi2dCommandOp::kBlit: {
        HBITMAP src = reinterpret_cast<HBITMAP>(cmd.blit.src_bitmap);
        backend->blit(src, cmd.blit.dest_x, cmd.blit.dest_y, cmd.blit.dest_w,
                    cmd.blit.dest_h, cmd.blit.src_x, cmd.blit.src_y,
                    cmd.blit.src_w, cmd.blit.src_h);
        note_write(target, cmd.blit.dest_x, cmd.blit.dest_y, cmd.blit.dest_w,
                   cmd.blit.dest_h);
        break;
      }
      case Rhi2dCommandOp::kClipRect: {
        HRGN rgn = ::CreateRectRgn(cmd.clip_rect.x, cmd.clip_rect.y,
                                   cmd.clip_rect.x + cmd.clip_rect.w,
                                   cmd.clip_rect.y + cmd.clip_rect.h);
        if (rgn) {
          ::SelectClipRgn(hdc, rgn);
          ::DeleteObject(rgn);
        }
        break;
      }
      case Rhi2dCommandOp::kResetClip:
        ::SelectClipRgn(hdc, nullptr);
        break;
      case Rhi2dCommandOp::kSetStyle:
        style_color = cmd.set_style.color;
        break;
      case Rhi2dCommandOp::kSetPen:
        backend->set_pen(cmd.set_pen);
        break;
      case Rhi2dCommandOp::kSetBrush:
        backend->set_brush(cmd.set_brush);
        break;
      case Rhi2dCommandOp::kPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.polyline.points));
        const int n = static_cast<int>(cmd.polyline.count);
        if (pts && n >= 2) {
          backend->polyline(pts, n);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case Rhi2dCommandOp::kPolyPolygon: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polygon.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polygon.counts));
        const int n_rings = static_cast<int>(cmd.poly_polygon.n_rings);
        if (pts && counts && n_rings >= 1) {
          backend->poly_polygon(pts, counts, n_rings);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case Rhi2dCommandOp::kPolyPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polyline.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polyline.counts));
        const int n_polys = static_cast<int>(cmd.poly_polyline.n_polys);
        if (pts && counts && n_polys >= 1) {
          backend->poly_polyline(pts, counts, n_polys);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case Rhi2dCommandOp::kEllipse:
        backend->ellipse(cmd.ellipse.left, cmd.ellipse.top, cmd.ellipse.right,
                       cmd.ellipse.bottom);
        note_write(target, cmd.ellipse.left, cmd.ellipse.top,
                   cmd.ellipse.right - cmd.ellipse.left,
                   cmd.ellipse.bottom - cmd.ellipse.top);
        break;
      case Rhi2dCommandOp::kText: {
        const auto* bytes = buffer.blob_at(cmd.text.text);
        if (bytes) {
          backend->draw_text_bytes(cmd.text.x, cmd.text.y,
                                 reinterpret_cast<const char*>(bytes),
                                 static_cast<int>(cmd.text.text.size),
                                 cmd.text.height_px, cmd.text.halo_px,
                                 cmd.text.angle_deg);
          note_write(target, cmd.text.x, cmd.text.y, cmd.text.height_px * 8,
                     cmd.text.height_px * 2);
        }
        break;
      }
      case Rhi2dCommandOp::kEndPass:
        break;
    }
  }

  backend->release_style();
  ::SelectObject(hdc, old);
  ::DeleteDC(hdc);
  return true;
}

bool execute_tile(const Rhi2dCommandBuffer& buffer, Rhi2dSurface& target,
                  int origin_x, int origin_y) {
  if (!target.bitmap || target.width < 1 || target.height < 1) {
    return false;
  }

  HDC hdc = ::CreateCompatibleDC(nullptr);
  if (!hdc) {
    return false;
  }
  HBITMAP old = static_cast<HBITMAP>(::SelectObject(hdc, target.bitmap));
  if (!old) {
    ::DeleteDC(hdc);
    return false;
  }

  const int tw = target.width;
  const int th = target.height;
  // Skirt for label halo / wide strokes so edge cull does not clip them.
  constexpr int kCullPad = 24;
  const int cull_ox = origin_x - kCullPad;
  const int cull_oy = origin_y - kCullPad;
  const int cull_tw = tw + 2 * kCullPad;
  const int cull_th = th + 2 * kCullPad;

  ::SetWindowOrgEx(hdc, origin_x, origin_y, nullptr);
  // Clip is in device coordinates (bitmap space), not logical.
  HRGN clip = ::CreateRectRgn(0, 0, tw, th);
  if (clip) {
    ::SelectClipRgn(hdc, clip);
    ::DeleteObject(clip);
  }

  ScopedPaintBackend backend(hdc);

  for (const Rhi2dCommand& cmd : buffer.ops()) {
    switch (cmd.op) {
      case Rhi2dCommandOp::kClear: {
        // Clear the tile bitmap in device space (ignore full-viewport extents).
        POINT old_org{};
        ::SetWindowOrgEx(hdc, 0, 0, &old_org);
        ::SelectClipRgn(hdc, nullptr);
        backend->clear_rect(0, 0, tw, th, cmd.clear.color);
        note_write(target, 0, 0, tw, th);
        ::SetWindowOrgEx(hdc, origin_x, origin_y, nullptr);
        HRGN reclip = ::CreateRectRgn(0, 0, tw, th);
        if (reclip) {
          ::SelectClipRgn(hdc, reclip);
          ::DeleteObject(reclip);
        }
        break;
      }
      case Rhi2dCommandOp::kFillRect:
        if (!rect_hits_tile(cmd.fill_rect.x, cmd.fill_rect.y,
                            cmd.fill_rect.x + cmd.fill_rect.w,
                            cmd.fill_rect.y + cmd.fill_rect.h, cull_ox, cull_oy,
                            cull_tw, cull_th)) {
          break;
        }
        backend->fill_rect(cmd.fill_rect.x, cmd.fill_rect.y, cmd.fill_rect.w,
                           cmd.fill_rect.h, cmd.fill_rect.color);
        break;
      case Rhi2dCommandOp::kStrokeRect:
        if (!rect_hits_tile(cmd.stroke_rect.x, cmd.stroke_rect.y,
                            cmd.stroke_rect.x + cmd.stroke_rect.w,
                            cmd.stroke_rect.y + cmd.stroke_rect.h, cull_ox,
                            cull_oy, cull_tw, cull_th)) {
          break;
        }
        backend->stroke_rect(cmd.stroke_rect.x, cmd.stroke_rect.y,
                             cmd.stroke_rect.w, cmd.stroke_rect.h,
                             cmd.stroke_rect.color, cmd.stroke_rect.width);
        break;
      case Rhi2dCommandOp::kBlit: {
        if (!rect_hits_tile(cmd.blit.dest_x, cmd.blit.dest_y,
                            cmd.blit.dest_x + cmd.blit.dest_w,
                            cmd.blit.dest_y + cmd.blit.dest_h, cull_ox, cull_oy,
                            cull_tw, cull_th)) {
          break;
        }
        HBITMAP src = reinterpret_cast<HBITMAP>(cmd.blit.src_bitmap);
        backend->blit(src, cmd.blit.dest_x, cmd.blit.dest_y, cmd.blit.dest_w,
                      cmd.blit.dest_h, cmd.blit.src_x, cmd.blit.src_y,
                      cmd.blit.src_w, cmd.blit.src_h);
        break;
      }
      case Rhi2dCommandOp::kClipRect: {
        HRGN rgn = ::CreateRectRgn(cmd.clip_rect.x, cmd.clip_rect.y,
                                   cmd.clip_rect.x + cmd.clip_rect.w,
                                   cmd.clip_rect.y + cmd.clip_rect.h);
        if (rgn) {
          ::ExtSelectClipRgn(hdc, rgn, RGN_AND);
          ::DeleteObject(rgn);
        }
        break;
      }
      case Rhi2dCommandOp::kResetClip: {
        HRGN reclip = ::CreateRectRgn(0, 0, tw, th);
        if (reclip) {
          ::SelectClipRgn(hdc, reclip);
          ::DeleteObject(reclip);
        }
        break;
      }
      case Rhi2dCommandOp::kSetStyle:
        break;
      case Rhi2dCommandOp::kSetPen:
        backend->set_pen(cmd.set_pen);
        break;
      case Rhi2dCommandOp::kSetBrush:
        backend->set_brush(cmd.set_brush);
        break;
      case Rhi2dCommandOp::kPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.polyline.points));
        const int n = static_cast<int>(cmd.polyline.count);
        if (pts && n >= 2 &&
            points_hit_tile(pts, n, cull_ox, cull_oy, cull_tw, cull_th)) {
          backend->polyline(pts, n);
        }
        break;
      }
      case Rhi2dCommandOp::kPolyPolygon: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polygon.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polygon.counts));
        const int n_rings = static_cast<int>(cmd.poly_polygon.n_rings);
        const int n_pts = total_point_count(counts, n_rings);
        if (pts && counts && n_rings >= 1 &&
            points_hit_tile(pts, n_pts, cull_ox, cull_oy, cull_tw, cull_th)) {
          backend->poly_polygon(pts, counts, n_rings);
        }
        break;
      }
      case Rhi2dCommandOp::kPolyPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polyline.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polyline.counts));
        const int n_polys = static_cast<int>(cmd.poly_polyline.n_polys);
        const int n_pts = total_point_count(counts, n_polys);
        if (pts && counts && n_polys >= 1 &&
            points_hit_tile(pts, n_pts, cull_ox, cull_oy, cull_tw, cull_th)) {
          backend->poly_polyline(pts, counts, n_polys);
        }
        break;
      }
      case Rhi2dCommandOp::kEllipse:
        if (!rect_hits_tile(cmd.ellipse.left, cmd.ellipse.top, cmd.ellipse.right,
                            cmd.ellipse.bottom, cull_ox, cull_oy, cull_tw,
                            cull_th)) {
          break;
        }
        backend->ellipse(cmd.ellipse.left, cmd.ellipse.top, cmd.ellipse.right,
                         cmd.ellipse.bottom);
        break;
      case Rhi2dCommandOp::kText: {
        const auto* bytes = buffer.blob_at(cmd.text.text);
        if (!bytes) {
          break;
        }
        const int pad = cmd.text.height_px * 8 + cmd.text.halo_px + 8;
        if (!rect_hits_tile(cmd.text.x - pad, cmd.text.y - pad,
                            cmd.text.x + pad, cmd.text.y + pad, cull_ox, cull_oy,
                            cull_tw, cull_th)) {
          break;
        }
        backend->draw_text_bytes(cmd.text.x, cmd.text.y,
                                 reinterpret_cast<const char*>(bytes),
                                 static_cast<int>(cmd.text.text.size),
                                 cmd.text.height_px, cmd.text.halo_px,
                                 cmd.text.angle_deg);
        break;
      }
      case Rhi2dCommandOp::kEndPass:
        break;
    }
  }

  backend->release_style();
  note_write(target, 0, 0, tw, th);
  ::SelectObject(hdc, old);
  ::DeleteDC(hdc);
  return true;
}

}  // namespace detail
}  // namespace scenic
