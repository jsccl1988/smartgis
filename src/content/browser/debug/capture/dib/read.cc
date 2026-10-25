// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/capture/dib/read.h"

#include "content/browser/debug/capture/pixel/bmp.h"
#include "content/browser/debug/capture/dib/gdi.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"

#include <vector>

namespace content {
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

// Raise the top-level owner above IDE chrome so CAPTUREBLT sees DXGI, not the
// desktop behind an occluded present HWND (same spirit as bring_hwnd_to_front).
HWND pin_capture_zorder(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return nullptr;
  }
  HWND root = GetAncestor(hwnd, GA_ROOT);
  if (!root || !IsWindow(root)) {
    root = hwnd;
  }
  ShowWindow(root, SW_SHOWNOACTIVATE);
  SetWindowPos(root, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  if (hwnd != root) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  }
  const HWND fg = GetForegroundWindow();
  if (fg && fg != root) {
    const DWORD my_tid = GetCurrentThreadId();
    DWORD fg_pid = 0;
    const DWORD fg_tid = GetWindowThreadProcessId(fg, &fg_pid);
    const bool attached =
        fg_tid && fg_tid != my_tid && AttachThreadInput(my_tid, fg_tid, TRUE);
    BringWindowToTop(root);
    SetForegroundWindow(root);
    if (attached) {
      AttachThreadInput(my_tid, fg_tid, FALSE);
    }
  } else {
    BringWindowToTop(root);
    SetForegroundWindow(root);
  }
  return root;
}

void unpin_capture_zorder(HWND root, HWND hwnd) {
  if (hwnd && IsWindow(hwnd) && hwnd != root) {
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  if (root && IsWindow(root)) {
    SetWindowPos(root, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
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
    // Stay TOPMOST for the whole attempt loop so IDE chrome cannot cover the
    // DXGI present rect mid-CAPTUREBLT (Python capture_flycube_present_bmp).
    const HWND root = pin_capture_zorder(hwnd);
    bool accepted = false;
    for (int attempt = 0; attempt < attempts; ++attempt) {
      pump_redraw(hwnd, opts, attempt);
      // DXGI flip-model swapchains often make PrintWindow return only the
      // render-pass clear (solid sky). Prefer a desktop BitBlt of the
      // DWM-composited HWND, then fall back to PrintWindow.
      if (client.blit_screen()) {
        got = client.read(&pixels, &header);
        if (frame_passes(pixels, got, width, height, opts)) {
          accepted = true;
          break;
        }
      }
      client.print_window();
      got = client.read(&pixels, &header);
      if (frame_passes(pixels, got, width, height, opts)) {
        accepted = true;
        break;
      }
    }
    unpin_capture_zorder(root, hwnd);

    // WS_EX_NOREDIRECTIONBITMAP FlyCube: a single black BitBlt used to still
    // write a full-size BMP; callers then lied with engine sidecars. Fail
    // closed unless a lit frame was accepted.
    if (!accepted) {
      return false;
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
}  // namespace content
