// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BLIT_FRAME_CACHE_H_
#define APP_VIEWS_BLIT_FRAME_CACHE_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "tool/camera_nav.h"

namespace app {

// Last-frame DIB for StretchBlt zoom/pan preview. Full paint is deferred
// until kBlitDebounceMs after the last gesture tick.
class BlitFrameCache {
 public:
  BlitFrameCache() = default;
  ~BlitFrameCache();

  BlitFrameCache(const BlitFrameCache&) = delete;
  BlitFrameCache& operator=(const BlitFrameCache&) = delete;

  void capture(HDC src, int w, int h);
  void begin_zoom(int view_w, int view_h, int cursor_x, int cursor_y,
                  double factor);
  void begin_pan(int view_w, int view_h, int dx_px, int dy_px);
  void end_preview();

  bool in_preview() const;
  bool has_frame() const { return dc_ != nullptr && bmp_ != nullptr; }

  // StretchBlt the last frame. Returns false when there is nothing to blit.
  bool present(HDC dst, int view_w, int view_h) const;

 private:
  void destroy();

  HDC dc_ = nullptr;
  HBITMAP bmp_ = nullptr;
  HBITMAP old_ = nullptr;
  int w_ = 0;
  int h_ = 0;
  bool preview_ = false;
  DWORD preview_until_ = 0;
  tool::BlitDestRect dest_{};
  double acc_factor_ = 1.0;
  int acc_dx_ = 0;
  int acc_dy_ = 0;
  int zoom_cx_ = 0;
  int zoom_cy_ = 0;
};

}  // namespace app

#endif  // APP_VIEWS_BLIT_FRAME_CACHE_H_
