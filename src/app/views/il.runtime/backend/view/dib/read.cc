// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/dib/read.h"

#include "app/views/il.runtime/backend/view/pixel/bmp.h"
#include "app/views/il.runtime/backend/view/dib/gdi.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"

#include <vector>

namespace app {
namespace detail {
namespace {

bool frame_passes(const std::vector<unsigned char>& pixels,
                  int got,
                  int width,
                  int height,
                  const CaptureOpts& opts) {
  if (got != height) {
    return false;
  }
  const int stride = bgr24_stride(width);
  if (!pixels_have_visible_signal(pixels.data(), stride, width, height,
                                  opts.visible)) {
    return false;
  }
  if (opts.require_shell_diversity &&
      !bmp_has_shell_diversity(pixels.data(), stride, width, height)) {
    return false;
  }
  return true;
}

void pump_redraw(HWND hwnd, const CaptureOpts& opts, int attempt) {
  if (opts.pump_base_ms == 0 && opts.pump_step_ms == 0) {
    return;
  }
  RedrawWindow(hwnd, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
  pump_messages(opts.pump_base_ms +
                static_cast<DWORD>(attempt) * opts.pump_step_ms);
}

}  // namespace

bool capture_hwnd_bmp(HWND hwnd,
                      const wchar_t* filename,
                      const CaptureOpts& opts) {
  if (!hwnd || !IsWindow(hwnd) || !filename) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;
  if (width < 8 || height < 8 || width > kMaxBmpEdge || height > kMaxBmpEdge) {
    return false;
  }
  if ((opts.dst_w != 0 || opts.dst_h != 0) &&
      (opts.dst_w > kMaxBmpEdge || opts.dst_h > kMaxBmpEdge)) {
    return false;
  }

  BITMAPINFOHEADER header = bgr24_header(width, height);
  std::vector<unsigned char> pixels;
  int got = 0;
  int write_h = height;
  {
    ClientDib client;
    if (!client.open(hwnd, width, height)) {
      return false;
    }

    const int attempts = opts.max_attempts < 1 ? 1 : opts.max_attempts;
    for (int attempt = 0; attempt < attempts; ++attempt) {
      pump_redraw(hwnd, opts, attempt);
      client.print_window();
      got = client.read(&pixels, &header);
      if (frame_passes(pixels, got, width, height, opts)) {
        break;
      }
      if (client.blit_screen()) {
        got = client.read(&pixels, &header);
      }
      if (frame_passes(pixels, got, width, height, opts)) {
        break;
      }
    }

    if (opts.dst_w >= 8 && opts.dst_h >= 8 &&
        (opts.dst_w != width || opts.dst_h != height)) {
      if (!client.stretch_read(opts.dst_w, opts.dst_h, &pixels, &header,
                               &got)) {
        return false;
      }
      write_h = opts.dst_h;
    }
  }

  if (got != write_h) {
    return false;
  }
  return write_bmp_file(filename, header, pixels.data(), pixels.size());
}

}  // namespace detail
}  // namespace app
