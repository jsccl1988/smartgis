// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/capture/label_composite.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>
#include <vector>
#include <windows.h>

namespace plugin {
namespace detail {

bool composite_legacy_labels_onto_bmp(content::Scene3dPresenter* cam,
                                      const wchar_t* bmp_path,
                                      int w,
                                      int h) {
  if (!cam || !bmp_path || w < 8 || h < 8) {
    return false;
  }
  HBITMAP hbmp = static_cast<HBITMAP>(LoadImageW(
      nullptr, bmp_path, IMAGE_BITMAP, 0, 0,
      LR_LOADFROMFILE | LR_CREATEDIBSECTION));
  if (!hbmp) {
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(hbmp);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, hbmp);
  cam->paint_legacy_place_labels(mem, w, h);

  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = w;
  bi.biHeight = -h;
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  const int got = GetDIBits(mem, hbmp, 0, h, pixels.data(),
                            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
  SelectObject(mem, old);
  DeleteDC(mem);
  DeleteObject(hbmp);
  if (got != h) {
    return false;
  }

  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());
  FILE* out = nullptr;
  if (_wfopen_s(&out, bmp_path, L"wb") != 0 || !out) {
    return false;
  }
  std::fwrite(&fh, sizeof(fh), 1, out);
  std::fwrite(&bi, sizeof(bi), 1, out);
  std::fwrite(pixels.data(), 1, pixels.size(), out);
  std::fclose(out);
  return true;
}

}  // namespace detail
}  // namespace plugin
