// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_CANVAS_STYLE_H_
#define SCENIC_GDI_CANVAS_STYLE_H_

#include "scenic/detail/style.h"
#include "scenic/detail/viewport.h"
#include "scenic/detail/err.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"

namespace scenic {
namespace detail {

// Owns GDI pen/brush/font/icon and the consecutive-feature style cache used
// by Rhi2dCartoDraw prepare/end/flush.
class Rhi2dCartoDrawStyle {
 public:
  Rhi2dCartoDrawStyle();
  ~Rhi2dCartoDrawStyle();

  Rhi2dCartoDrawStyle(const Rhi2dCartoDrawStyle&) = delete;
  Rhi2dCartoDrawStyle& operator=(const Rhi2dCartoDrawStyle&) = delete;

  int prepare_for_drawing(HDC dc, bool recording, HINSTANCE h_inst,
                          RenderContext* rc, bool is_river, int road_class,
                          const Style* style, int draw_mode = R2_COPYPEN);
  int end_drawing(HDC dc);
  // Drop cached pen/brush from the DC (call at end of map/layer paint).
  void flush(HDC dc);
  void release_from_dc(HDC dc);
  void invalidate_cache();

  HICON icon() const { return h_icon_; }

 private:
  HFONT h_font_ = nullptr;
  HPEN h_pen_ = nullptr;
  HBRUSH h_brush_ = nullptr;
  HICON h_icon_ = nullptr;

  HFONT h_old_font_ = nullptr;
  HPEN h_old_pen_ = nullptr;
  HBRUSH h_old_brush_ = nullptr;
  bool cur_use_style_ = false;

  // Skip ExtCreatePen / CreateBrush when consecutive features share
  // stroke/fill. When cache hits, keep pen/brush selected on the DC across
  // features.
  bool style_cache_valid_ = false;
  bool style_on_dc_ = false;
  int cache_draw_mode_ = 0;
  ulong cache_format_ = 0;
  int cache_road_class_ = -1;
  int cache_is_river_ = 0;
  int cache_pen_px_ = 0;
  COLORREF cache_pen_ = 0;
  COLORREF cache_fill_ = 0;
  int cache_brush_tp_ = -1;
  int cache_brush_style_ = 0;

  void destroy_objects();
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_CANVAS_STYLE_H_
