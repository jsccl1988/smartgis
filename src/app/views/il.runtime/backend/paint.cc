// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/paint.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

bool write_software_scene3d_bmp(content::Scene3dPresenter* cam,
                                const wchar_t* bmp_w,
                                int w,
                                int h) {
  if (!cam || !bmp_w || w < 8 || h < 8) {
    return false;
  }
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  bool painted = false;
  try {
    cam->software().paint(mem, w, h, true);
    painted = true;
  } catch (...) {
    painted = false;
  }
  SelectObject(mem, old);
  bool wrote = false;
  if (painted) {
    const size_t image_bytes =
        static_cast<size_t>(w) * 4u * static_cast<size_t>(h);
    BITMAPINFOHEADER bih = bmi.bmiHeader;
    bih.biSizeImage = static_cast<DWORD>(image_bytes);
    wrote = write_bmp_file(bmp_w, bih, bits, image_bytes);
  }
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return wrote;
}

}  // namespace detail
}  // namespace app
