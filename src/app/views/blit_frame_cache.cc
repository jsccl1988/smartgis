// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/blit_frame_cache.h"

namespace app {

BlitFrameCache::~BlitFrameCache() { destroy(); }

void BlitFrameCache::destroy() {
  if (dc_) {
    if (old_) {
      SelectObject(dc_, old_);
    }
    DeleteDC(dc_);
    dc_ = nullptr;
    old_ = nullptr;
  }
  if (bmp_) {
    DeleteObject(bmp_);
    bmp_ = nullptr;
  }
  w_ = 0;
  h_ = 0;
  preview_ = false;
  preview_until_ = 0;
}

void BlitFrameCache::capture(HDC src, int w, int h) {
  if (!src || w <= 0 || h <= 0) {
    return;
  }
  if (w != w_ || h != h_ || !dc_ || !bmp_) {
    destroy();
    dc_ = CreateCompatibleDC(src);
    if (!dc_) {
      return;
    }
    bmp_ = CreateCompatibleBitmap(src, w, h);
    if (!bmp_) {
      DeleteDC(dc_);
      dc_ = nullptr;
      return;
    }
    old_ = static_cast<HBITMAP>(SelectObject(dc_, bmp_));
    w_ = w;
    h_ = h;
  }
  BitBlt(dc_, 0, 0, w, h, src, 0, 0, SRCCOPY);
  preview_ = false;
  acc_factor_ = 1.0;
  acc_dx_ = 0;
  acc_dy_ = 0;
}

void BlitFrameCache::begin_zoom(int view_w, int view_h, int cursor_x,
                                int cursor_y, double factor) {
  if (!preview_) {
    acc_factor_ = 1.0;
    zoom_cx_ = cursor_x;
    zoom_cy_ = cursor_y;
  }
  acc_factor_ *= factor;
  dest_ = tool::zoom_blit_dest(view_w, view_h, zoom_cx_, zoom_cy_, acc_factor_);
  preview_ = has_frame();
  preview_until_ = GetTickCount() + static_cast<DWORD>(tool::kBlitDebounceMs);
}

void BlitFrameCache::begin_pan(int view_w, int view_h, int dx_px, int dy_px) {
  if (!preview_) {
    acc_dx_ = 0;
    acc_dy_ = 0;
  }
  acc_dx_ += dx_px;
  acc_dy_ += dy_px;
  dest_ = tool::pan_blit_dest(view_w, view_h, acc_dx_, acc_dy_);
  preview_ = has_frame();
  preview_until_ = GetTickCount() + static_cast<DWORD>(tool::kBlitDebounceMs);
}

void BlitFrameCache::end_preview() {
  preview_ = false;
  preview_until_ = 0;
}

bool BlitFrameCache::in_preview() const {
  return preview_ && preview_until_ != 0 && GetTickCount() < preview_until_;
}

bool BlitFrameCache::present(HDC dst, int view_w, int view_h) const {
  if (!dst || !preview_ || !has_frame() || view_w <= 0 || view_h <= 0) {
    return false;
  }
  RECT full = {0, 0, view_w, view_h};
  HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
  FillRect(dst, &full, bg);
  DeleteObject(bg);
  SetStretchBltMode(dst, HALFTONE);
  SetBrushOrgEx(dst, 0, 0, nullptr);
  StretchBlt(dst, dest_.x, dest_.y, dest_.w, dest_.h, dc_, 0, 0, w_, h_,
             SRCCOPY);
  return true;
}

}  // namespace app
