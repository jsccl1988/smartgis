// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/dib/gdi.h"

#ifndef PW_CLIENTONLY
#define PW_CLIENTONLY 0x00000001
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace app {
namespace detail {

// GetDC / ReleaseDC pair. Does not own the HWND.
class WindowDc {
 public:
  explicit WindowDc(HWND hwnd) : hwnd_(hwnd), dc_(GetDC(hwnd)) {}

  ~WindowDc() {
    if (dc_) {
      ReleaseDC(hwnd_, dc_);
    }
  }

  WindowDc(const WindowDc&) = delete;
  WindowDc& operator=(const WindowDc&) = delete;

  HDC get() const { return dc_; }
  explicit operator bool() const { return dc_ != nullptr; }

 private:
  HWND hwnd_ = nullptr;
  HDC dc_ = nullptr;
};

// Compatible bitmap selected into its own memory DC.
// Restores the previous object before delete; GDI rejects deleting a
// selected bitmap.
class MemBmp {
 public:
  MemBmp(HDC reference, int width, int height)
      : width_(width), height_(height) {
    dc_ = CreateCompatibleDC(reference);
    bitmap_ = CreateCompatibleBitmap(reference, width_, height_);
    if (dc_ && bitmap_) {
      previous_ = SelectObject(dc_, bitmap_);
    }
  }

  ~MemBmp() { release(); }

  MemBmp(const MemBmp&) = delete;
  MemBmp& operator=(const MemBmp&) = delete;

  bool ready() const { return dc_ != nullptr && bitmap_ != nullptr; }
  HDC dc() const { return dc_; }
  HBITMAP bitmap() const { return bitmap_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  void release() {
    if (dc_ && previous_) {
      SelectObject(dc_, previous_);
    }
    if (bitmap_) {
      DeleteObject(bitmap_);
    }
    if (dc_) {
      DeleteDC(dc_);
    }
    dc_ = nullptr;
    bitmap_ = nullptr;
    previous_ = nullptr;
  }

  HDC dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ previous_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

void print_client(HWND hwnd, HDC mem) {
  if (!PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY)) {
    (void)PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
  }
}

int read_bgr24(const MemBmp& dib,
               std::vector<unsigned char>* pixels,
               BITMAPINFOHEADER* header) {
  const int stride = bgr24_stride(dib.width());
  const size_t nbytes =
      static_cast<size_t>(stride) * static_cast<size_t>(dib.height());
  if (pixels->size() != nbytes) {
    pixels->assign(nbytes, 0);
  }
  return GetDIBits(dib.dc(), dib.bitmap(), 0, static_cast<UINT>(dib.height()),
                   pixels->data(), reinterpret_cast<BITMAPINFO*>(header),
                   DIB_RGB_COLORS);
}

struct ClientDib::State {
  HWND hwnd = nullptr;
  WindowDc* window = nullptr;
  MemBmp* client = nullptr;
  int width = 0;
  int height = 0;

  ~State() {
    delete client;
    delete window;
  }

  State(const State&) = delete;
  State& operator=(const State&) = delete;
  State() = default;
};

bool blit_client_to_dib(HWND hwnd, HDC mem, int w, int h) {
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  POINT origin = {0, 0};
  ClientToScreen(hwnd, &origin);
  // Flip-model DXGI (WS_EX_NOREDIRECTIONBITMAP) has no GDI redirection
  // bitmap. Plain SRCCOPY often returns the pass clear (solid sky) while the
  // GPU limb sits only in a corner strip. CAPTUREBLT reads the DWM-composited
  // client — same pattern as horizon atom capture.
  const BOOL ok =
      BitBlt(mem, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT);
  ReleaseDC(nullptr, screen);
  return ok != FALSE;
}

ClientDib::ClientDib() = default;

ClientDib::~ClientDib() {
  delete state_;
}

bool ClientDib::open(HWND hwnd, int width, int height) {
  delete state_;
  state_ = nullptr;
  if (!hwnd || width < 1 || height < 1) {
    return false;
  }
  State* state = new State();
  state->hwnd = hwnd;
  state->width = width;
  state->height = height;
  state->window = new WindowDc(hwnd);
  if (!*state->window) {
    delete state;
    return false;
  }
  state->client = new MemBmp(state->window->get(), width, height);
  if (!state->client->ready()) {
    delete state;
    return false;
  }
  state_ = state;
  return true;
}

int ClientDib::width() const {
  return state_ ? state_->width : 0;
}

int ClientDib::height() const {
  return state_ ? state_->height : 0;
}

void ClientDib::print_window() {
  if (!state_ || !state_->client) {
    return;
  }
  print_client(state_->hwnd, state_->client->dc());
}

bool ClientDib::blit_screen() {
  if (!state_ || !state_->client) {
    return false;
  }
  return blit_client_to_dib(state_->hwnd, state_->client->dc(), state_->width,
                            state_->height);
}

int ClientDib::read(std::vector<unsigned char>* pixels,
                    BITMAPINFOHEADER* header) {
  if (!state_ || !state_->client || !pixels || !header) {
    return 0;
  }
  return read_bgr24(*state_->client, pixels, header);
}

bool ClientDib::stretch_read(int dst_w,
                             int dst_h,
                             std::vector<unsigned char>* pixels,
                             BITMAPINFOHEADER* header,
                             int* rows) {
  if (!state_ || !state_->window || !state_->client || !pixels || !header ||
      !rows) {
    return false;
  }
  MemBmp dst(state_->window->get(), dst_w, dst_h);
  if (!dst.ready()) {
    return false;
  }
  SetStretchBltMode(dst.dc(), HALFTONE);
  SetBrushOrgEx(dst.dc(), 0, 0, nullptr);
  if (!StretchBlt(dst.dc(), 0, 0, dst_w, dst_h, state_->client->dc(), 0, 0,
                  state_->width, state_->height, SRCCOPY)) {
    return false;
  }
  header->biWidth = dst_w;
  header->biHeight = -dst_h;
  *rows = read_bgr24(dst, pixels, header);
  return true;
}

}  // namespace detail
}  // namespace app
