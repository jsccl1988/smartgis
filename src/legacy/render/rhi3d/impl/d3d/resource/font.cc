// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

#include <cstring>
#include <string>
#include <vector>

namespace render {

long SmtD3DRenderDevice::CreateFont(const char* szChType, int nHeight,
                                    int nWidth, int nWeight, bool bItalic,
                                    bool bUnderline, bool bStrike, ulong dwSize,
                                    uint& unID) {
  HDC hdc = hwnd_ ? ::GetDC(hwnd_) : ::CreateCompatibleDC(nullptr);
  if (!hdc) {
    return SMT_ERR_FAILURE;
  }

  int height = nHeight;
  int width = nWidth;
  if (height == 0 && width == 0) {
    height =
        MulDiv(static_cast<int>(dwSize), ::GetDeviceCaps(hdc, LOGPIXELSY), 72);
  }

  const char* face = (szChType && szChType[0]) ? szChType : "Arial";
  HFONT font =
      ::CreateFontA(height, width, 0, 0, nWeight, bItalic ? TRUE : FALSE,
                    bUnderline ? TRUE : FALSE, bStrike ? TRUE : FALSE,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH, face);
  if (hwnd_) {
    ::ReleaseDC(hwnd_, hdc);
  } else {
    ::DeleteDC(hdc);
  }
  if (!font) {
    return SMT_ERR_FAILURE;
  }

  D3dFontSlot slot;
  slot.font = font;
  slot.height_px = (height > 0) ? height : 16;
  fonts_.push_back(slot);
  unID = static_cast<uint>(fonts_.size() - 1);
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::draw_text_gdi(uint font_id, float xscreen,
                                       float yscreen, const SmtColor& color,
                                       const char* text) {
  if (!text || font_id >= fonts_.size() || !fonts_[font_id].font) {
    return SMT_ERR_INVALID_PARAM;
  }
  const int len = static_cast<int>(std::strlen(text));
  if (len <= 0) {
    return SMT_ERR_NONE;
  }

  HDC screen = ::GetDC(nullptr);
  HDC mem = ::CreateCompatibleDC(screen);
  if (!mem) {
    if (screen) ::ReleaseDC(nullptr, screen);
    return SMT_ERR_FAILURE;
  }
  HFONT old = static_cast<HFONT>(::SelectObject(mem, fonts_[font_id].font));
  SIZE sz = {};
  ::GetTextExtentPoint32A(mem, text, len, &sz);
  if (sz.cx <= 0 || sz.cy <= 0) {
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    if (screen) ::ReleaseDC(nullptr, screen);
    return SMT_ERR_FAILURE;
  }

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = sz.cx;
  bmi.bmiHeader.biHeight = -sz.cy;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      ::CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    if (screen) ::ReleaseDC(nullptr, screen);
    return SMT_ERR_FAILURE;
  }
  HBITMAP old_bmp = static_cast<HBITMAP>(::SelectObject(mem, dib));
  ::SetBkMode(mem, TRANSPARENT);
  ::SetTextColor(mem, RGB(static_cast<int>(color.fRed * 255.f),
                          static_cast<int>(color.fGreen * 255.f),
                          static_cast<int>(color.fBlue * 255.f)));
  std::memset(bits, 0, static_cast<size_t>(sz.cx) * sz.cy * 4u);
  ::TextOutA(mem, 0, 0, text, len);

  // Convert GDI BGRA (opaque text on zero alpha) to premultiplied-ish BGRA
  // with alpha from luminance so DrawScreenBgra can blend.
  auto* px = static_cast<unsigned char*>(bits);
  for (int i = 0; i < sz.cx * sz.cy; ++i) {
    const unsigned char b = px[i * 4 + 0];
    const unsigned char g = px[i * 4 + 1];
    const unsigned char r = px[i * 4 + 2];
    const unsigned char a =
        static_cast<unsigned char>((static_cast<int>(r) + g + b) / 3);
    px[i * 4 + 3] = a;
  }

  const long rc = DrawScreenBgra(xscreen + sz.cx * 0.5f, yscreen + sz.cy * 0.5f,
                                 sz.cx, sz.cy, px);

  ::SelectObject(mem, old_bmp);
  ::SelectObject(mem, old);
  ::DeleteObject(dib);
  ::DeleteDC(mem);
  if (screen) ::ReleaseDC(nullptr, screen);
  return rc;
}

}  // namespace render
