// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"

#include <utility>

#include "legacy/render/rhi2d/impl/gdi/paint/player/gdi_player.h"

namespace render {
namespace detail {
namespace {

void note_write(GdiSurface& target, int x, int y, int w, int h) {
  target.bump_generation();
  target.mark_dirty(x, y, w, h);
}

}  // namespace

bool GdiCommandEncoder::begin_pass(GdiSurface* target) {
  if (!target || recording_) {
    return false;
  }
  target_ = target;
  recording_ = true;
  buffer_.clear();
  return true;
}

bool GdiCommandEncoder::end_pass() {
  if (!recording_) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kEndPass;
  buffer_.push(cmd);
  recording_ = false;
  target_ = nullptr;
  return true;
}

bool GdiCommandEncoder::push_cmd(GdiCommand cmd) {
  if (!recording_) {
    return false;
  }
  buffer_.push(cmd);
  return true;
}

GdiBlobRange GdiCommandEncoder::append_blob(const void* data, size_t n) {
  if (!recording_) {
    return {};
  }
  return buffer_.append_blob(data, n);
}

bool GdiCommandEncoder::clear(COLORREF color) {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kClear;
  cmd.clear.color = color;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::fill_rect(int x, int y, int w, int h, COLORREF color) {
  if (w < 1 || h < 1) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kFillRect;
  cmd.fill_rect.x = x;
  cmd.fill_rect.y = y;
  cmd.fill_rect.w = w;
  cmd.fill_rect.h = h;
  cmd.fill_rect.color = color;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::stroke_rect(int x, int y, int w, int h, COLORREF color,
                                    int pen_width) {
  if (w < 1 || h < 1) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kStrokeRect;
  cmd.stroke_rect.x = x;
  cmd.stroke_rect.y = y;
  cmd.stroke_rect.w = w;
  cmd.stroke_rect.h = h;
  cmd.stroke_rect.color = color;
  cmd.stroke_rect.width = pen_width < 1 ? 1 : pen_width;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::clip_rect(int x, int y, int w, int h) {
  if (w < 1 || h < 1) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kClipRect;
  cmd.clip_rect.x = x;
  cmd.clip_rect.y = y;
  cmd.clip_rect.w = w;
  cmd.clip_rect.h = h;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::reset_clip() {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kResetClip;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::blit(HBITMAP src, int dest_x, int dest_y, int dest_w,
                             int dest_h, int src_x, int src_y, int src_w,
                             int src_h) {
  if (!src || dest_w < 1 || dest_h < 1 || src_w < 1 || src_h < 1) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kBlit;
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

bool GdiCommandEncoder::set_style(uint32_t style_id, COLORREF color) {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kSetStyle;
  cmd.set_style.style_id = style_id;
  cmd.set_style.color = color;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::set_pen(COLORREF color, int width, int style) {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kSetPen;
  cmd.set_pen.color = color;
  cmd.set_pen.width = width < 1 ? 1 : width;
  cmd.set_pen.style = style;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::set_brush(COLORREF color, int style, int hatch) {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kSetBrush;
  cmd.set_brush.color = color;
  cmd.set_brush.style = style;
  cmd.set_brush.hatch = hatch;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::polyline(const POINT* pts, int count) {
  if (!pts || count < 2) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kPolyline;
  cmd.polyline.count = static_cast<uint32_t>(count);
  cmd.polyline.points =
      append_blob(pts, static_cast<size_t>(count) * sizeof(POINT));
  if (cmd.polyline.points.size == 0) {
    return false;
  }
  return push_cmd(cmd);
}

bool GdiCommandEncoder::poly_polygon(const POINT* pts, const int* ring_counts,
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
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kPolyPolygon;
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

bool GdiCommandEncoder::poly_polyline(const POINT* pts, const int* poly_counts,
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
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kPolyPolyline;
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

bool GdiCommandEncoder::ellipse(int left, int top, int right, int bottom) {
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kEllipse;
  cmd.ellipse.left = left;
  cmd.ellipse.top = top;
  cmd.ellipse.right = right;
  cmd.ellipse.bottom = bottom;
  return push_cmd(cmd);
}

bool GdiCommandEncoder::text(int x, int y, const char* text, int n,
                             int height_px, int halo_px, float angle_deg) {
  if (!text || n < 1) {
    return false;
  }
  GdiCommand cmd;
  cmd.op = GdiCommandOp::kText;
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

GdiCommandBuffer GdiCommandEncoder::take_buffer() {
  recording_ = false;
  target_ = nullptr;
  return std::move(buffer_);
}

bool replay(const GdiCommandBuffer& buffer, GdiSurface& target) {
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

  GdiPlayer player(hdc);
  [[maybe_unused]] COLORREF style_color = RGB(0, 0, 0);

  for (const GdiCommand& cmd : buffer.ops()) {
    switch (cmd.op) {
      case GdiCommandOp::kClear:
        player.clear_rect(0, 0, target.width, target.height, cmd.clear.color);
        note_write(target, 0, 0, target.width, target.height);
        break;
      case GdiCommandOp::kFillRect:
        player.fill_rect(cmd.fill_rect.x, cmd.fill_rect.y, cmd.fill_rect.w,
                         cmd.fill_rect.h, cmd.fill_rect.color);
        note_write(target, cmd.fill_rect.x, cmd.fill_rect.y, cmd.fill_rect.w,
                   cmd.fill_rect.h);
        break;
      case GdiCommandOp::kStrokeRect:
        player.stroke_rect(cmd.stroke_rect.x, cmd.stroke_rect.y,
                           cmd.stroke_rect.w, cmd.stroke_rect.h,
                           cmd.stroke_rect.color, cmd.stroke_rect.width);
        note_write(target, cmd.stroke_rect.x, cmd.stroke_rect.y,
                   cmd.stroke_rect.w, cmd.stroke_rect.h);
        break;
      case GdiCommandOp::kBlit: {
        HBITMAP src = reinterpret_cast<HBITMAP>(cmd.blit.src_bitmap);
        player.blit(src, cmd.blit.dest_x, cmd.blit.dest_y, cmd.blit.dest_w,
                    cmd.blit.dest_h, cmd.blit.src_x, cmd.blit.src_y,
                    cmd.blit.src_w, cmd.blit.src_h);
        note_write(target, cmd.blit.dest_x, cmd.blit.dest_y, cmd.blit.dest_w,
                   cmd.blit.dest_h);
        break;
      }
      case GdiCommandOp::kClipRect: {
        HRGN rgn = ::CreateRectRgn(cmd.clip_rect.x, cmd.clip_rect.y,
                                   cmd.clip_rect.x + cmd.clip_rect.w,
                                   cmd.clip_rect.y + cmd.clip_rect.h);
        if (rgn) {
          ::SelectClipRgn(hdc, rgn);
          ::DeleteObject(rgn);
        }
        break;
      }
      case GdiCommandOp::kResetClip:
        ::SelectClipRgn(hdc, nullptr);
        break;
      case GdiCommandOp::kSetStyle:
        style_color = cmd.set_style.color;
        break;
      case GdiCommandOp::kSetPen:
        player.set_pen(cmd.set_pen);
        break;
      case GdiCommandOp::kSetBrush:
        player.set_brush(cmd.set_brush);
        break;
      case GdiCommandOp::kPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.polyline.points));
        const int n = static_cast<int>(cmd.polyline.count);
        if (pts && n >= 2) {
          player.polyline(pts, n);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case GdiCommandOp::kPolyPolygon: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polygon.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polygon.counts));
        const int n_rings = static_cast<int>(cmd.poly_polygon.n_rings);
        if (pts && counts && n_rings >= 1) {
          player.poly_polygon(pts, counts, n_rings);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case GdiCommandOp::kPolyPolyline: {
        const auto* pts = reinterpret_cast<const POINT*>(
            buffer.blob_at(cmd.poly_polyline.points));
        const auto* counts = reinterpret_cast<const int*>(
            buffer.blob_at(cmd.poly_polyline.counts));
        const int n_polys = static_cast<int>(cmd.poly_polyline.n_polys);
        if (pts && counts && n_polys >= 1) {
          player.poly_polyline(pts, counts, n_polys);
          note_write(target, 0, 0, target.width, target.height);
        }
        break;
      }
      case GdiCommandOp::kEllipse:
        player.ellipse(cmd.ellipse.left, cmd.ellipse.top, cmd.ellipse.right,
                       cmd.ellipse.bottom);
        note_write(target, cmd.ellipse.left, cmd.ellipse.top,
                   cmd.ellipse.right - cmd.ellipse.left,
                   cmd.ellipse.bottom - cmd.ellipse.top);
        break;
      case GdiCommandOp::kText: {
        const auto* bytes = buffer.blob_at(cmd.text.text);
        if (bytes) {
          player.draw_text_bytes(cmd.text.x, cmd.text.y,
                                 reinterpret_cast<const char*>(bytes),
                                 static_cast<int>(cmd.text.text.size),
                                 cmd.text.height_px, cmd.text.halo_px,
                                 cmd.text.angle_deg);
          note_write(target, cmd.text.x, cmd.text.y, cmd.text.height_px * 8,
                     cmd.text.height_px * 2);
        }
        break;
      }
      case GdiCommandOp::kEndPass:
        break;
    }
  }

  player.release_style();
  ::SelectObject(hdc, old);
  ::DeleteDC(hdc);
  return true;
}

}  // namespace detail
}  // namespace render
