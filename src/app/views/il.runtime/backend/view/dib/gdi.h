// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_DIB_GDI_H_
#define IL_RUNTIME_BACKEND_VIEW_DIB_GDI_H_

#include <vector>
#include <windows.h>

namespace app {
namespace detail {

inline int bgr24_stride(int width) {
  return ((width * 3 + 3) / 4) * 4;
}

inline BITMAPINFOHEADER bgr24_header(int width, int height) {
  BITMAPINFOHEADER header{};
  header.biSize = sizeof(header);
  header.biWidth = width;
  header.biHeight = -height;  // top-down
  header.biPlanes = 1;
  header.biBitCount = 24;
  header.biCompression = BI_RGB;
  return header;
}

// Screen BitBlt of the HWND client into |mem|. Flip-model DXGI often has no
// GDI bitmap; desktop pixels at the client origin are the working fallback.
bool blit_client_to_dib(HWND hwnd, HDC mem, int w, int h);

// One client-area compatible bitmap. Each method is a single GDI step.
// Retry counts, lit-signal gates, and the BMP file write stay in read.
class ClientDib {
 public:
  ClientDib();
  ~ClientDib();

  ClientDib(const ClientDib&) = delete;
  ClientDib& operator=(const ClientDib&) = delete;

  // Client size must already be in range. False when GDI alloc fails.
  bool open(HWND hwnd, int width, int height);
  int width() const;
  int height() const;
  void print_window();
  bool blit_screen();
  // GetDIBits into |pixels|. Returns the row count.
  int read(std::vector<unsigned char>* pixels, BITMAPINFOHEADER* header);
  // Halftone stretch, then read. False leaves |pixels| / |header| / |rows|
  // unchanged.
  bool stretch_read(int dst_w,
                    int dst_h,
                    std::vector<unsigned char>* pixels,
                    BITMAPINFOHEADER* header,
                    int* rows);

 private:
  struct State;
  State* state_ = nullptr;
};

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_DIB_GDI_H_
