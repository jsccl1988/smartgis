// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Offscreen DIB for one-BitBlt present + overlay compose.

#include "ui/views/map/viewport/draw_host.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "ui/views/map/frame/embed_fill.h"

namespace ui {
namespace views {

void DrawHost::release_backbuffer() {
  if (back_dc_) {
    if (back_old_) {
      SelectObject(back_dc_, back_old_);
      back_old_ = nullptr;
    }
    DeleteDC(back_dc_);
    back_dc_ = nullptr;
  }
  if (back_dib_) {
    DeleteObject(back_dib_);
    back_dib_ = nullptr;
  }
  back_w_ = 0;
  back_h_ = 0;
}

bool DrawHost::ensure_backbuffer(int width_px, int height_px) {
  if (width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (back_dc_ && back_dib_ && back_w_ == width_px && back_h_ == height_px) {
    return true;
  }
  release_backbuffer();

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(dib);
    return false;
  }
  HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, dib));
  back_dc_ = mem;
  back_dib_ = dib;
  back_old_ = old;
  back_w_ = width_px;
  back_h_ = height_px;
  // Size change discards the last composited frame; require a fresh present
  // (or placeholder) before BitBlt so we never show an empty DIB.
  painted_generation_ = 0;
  RECT fill = {0, 0, width_px, height_px};
  detail::fill_map_embed_opaque(mem, fill, role_ == Role::kScene3d);
  return true;
}

bool DrawHost::export_bmp(const std::string& path) const {
  if (path.empty() || !frame_ready_ || !back_dib_ || !back_dc_ || back_w_ <= 0 ||
      back_h_ <= 0) {
    return false;
  }
  const int width = back_w_;
  const int height = back_h_;
  const int stride = width * 4;
  std::vector<std::uint8_t> pixels(static_cast<size_t>(stride) * height);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  if (!GetDIBits(back_dc_, back_dib_, 0, static_cast<UINT>(height),
                 pixels.data(), &bmi, DIB_RGB_COLORS)) {
    return false;
  }
  const std::uint32_t pixel_bytes =
      static_cast<std::uint32_t>(stride) * static_cast<std::uint32_t>(height);
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  const std::uint32_t file_size = 54u + pixel_bytes;
  const unsigned char header[54] = {
      'B', 'M',
      static_cast<unsigned char>(file_size),
      static_cast<unsigned char>(file_size >> 8),
      static_cast<unsigned char>(file_size >> 16),
      static_cast<unsigned char>(file_size >> 24),
      0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
      static_cast<unsigned char>(width),
      static_cast<unsigned char>(width >> 8),
      static_cast<unsigned char>(width >> 16),
      static_cast<unsigned char>(width >> 24),
      static_cast<unsigned char>(height),
      static_cast<unsigned char>(height >> 8),
      static_cast<unsigned char>(height >> 16),
      static_cast<unsigned char>(height >> 24),
      1, 0, 32, 0, 0, 0, 0, 0,
      static_cast<unsigned char>(pixel_bytes),
      static_cast<unsigned char>(pixel_bytes >> 8),
      static_cast<unsigned char>(pixel_bytes >> 16),
      static_cast<unsigned char>(pixel_bytes >> 24),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  out.write(reinterpret_cast<const char*>(header), 54);
  out.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
  return static_cast<bool>(out);
}

}  // namespace views
}  // namespace ui
