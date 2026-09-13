// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "map_pane.h"

#include "detail/local_render.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace app {
namespace webview2 {
namespace {

const wchar_t kClass[] = L"SmartGisWebMapPane";

uint64_t qpc_now() {
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  return static_cast<uint64_t>(q.QuadPart);
}

bool blit_shared_dib(HDC hdc, int w, int h, const content::SharedSurface& s) {
  if (!s.nt_handle || s.width_px == 0 || s.height_px == 0) {
    return false;
  }
  HANDLE mapping = static_cast<HANDLE>(s.nt_handle);
  const SIZE_T bytes =
      static_cast<SIZE_T>(s.width_px) * static_cast<SIZE_T>(s.height_px) * 4;
  void* bits = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, bytes);
  if (!bits) {
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = static_cast<LONG>(s.width_px);
  bmi.bmiHeader.biHeight = -static_cast<LONG>(s.height_px);
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  StretchDIBits(hdc, 0, 0, w, h, 0, 0, static_cast<int>(s.width_px),
                static_cast<int>(s.height_px), bits, &bmi, DIB_RGB_COLORS,
                SRCCOPY);
  UnmapViewOfFile(bits);
  return true;
}

}  // namespace

MapPane::MapPane() = default;

MapPane::~MapPane() {
  if (hwnd_) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

bool MapPane::create(HWND parent,
                     content::MapContents* session,
                     uint32_t view_id) {
  session_ = session;
  view_id_ = view_id;

  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &MapPane::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    registered = true;
  }

  hwnd_ = CreateWindowExW(0, kClass, L"Map",
                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                          parent, nullptr, GetModuleHandleW(nullptr), this);
  return hwnd_ != nullptr;
}

void MapPane::move(int x, int y, int w, int h, float dpi) {
  if (!hwnd_) {
    return;
  }
  SetWindowPos(hwnd_, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  if (session_) {
    if (content::MapWidgetHostView* view = session_->HostView(view_id_)) {
      view->Resize(x, y, w, h, dpi);
    }
  }
}

void MapPane::invalidate() {
  if (hwnd_) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void MapPane::set_oop(bool oop) {
  oop_ = oop;
  invalidate();
}

void MapPane::set_probe_text(const wchar_t* text) {
  probe_ = text ? text : L"";
  invalidate();
}

LRESULT CALLBACK MapPane::wnd_proc(HWND hwnd,
                                   UINT msg,
                                   WPARAM wp,
                                   LPARAM lp) {
  MapPane* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    self = static_cast<MapPane*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<MapPane*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) {
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  return self->handle(msg, wp, lp);
}

LRESULT MapPane::handle(UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_PAINT:
      paint();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
      dispatch_mouse(msg, wp, lp);
      return 0;
    default:
      return DefWindowProcW(hwnd_, msg, wp, lp);
  }
}

void MapPane::paint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  RECT rc;
  GetClientRect(hwnd_, &rc);
  present_or_fallback(hdc, rc.right - rc.left, rc.bottom - rc.top);
  EndPaint(hwnd_, &ps);
}

void MapPane::present_or_fallback(HDC hdc, int w, int h) {
  if (session_) {
    if (content::MapWidgetHostView* view = session_->HostView(view_id_)) {
      if (blit_shared_dib(hdc, w, h, view->Latest())) {
        return;
      }
    }
    oop_ = session_->IsOopRender();
    if (session_->PresentStatus() && session_->PresentStatus()[0]) {
      probe_ = session_->PresentStatus();
    }
  }

  RECT rc{0, 0, w, h};
  HBRUSH bg = CreateSolidBrush(RGB(18, 32, 48));
  FillRect(hdc, &rc, bg);
  DeleteObject(bg);

  HPEN grid = CreatePen(PS_SOLID, 1, RGB(40, 72, 96));
  HGDIOBJ old = SelectObject(hdc, grid);
  for (int x = 0; x < w; x += 48) {
    MoveToEx(hdc, x, 0, nullptr);
    LineTo(hdc, x, h);
  }
  for (int y = 0; y < h; y += 48) {
    MoveToEx(hdc, 0, y, nullptr);
    LineTo(hdc, w, y);
  }
  HPEN axis = CreatePen(PS_SOLID, 2, RGB(80, 160, 210));
  SelectObject(hdc, axis);
  MoveToEx(hdc, 0, h / 2, nullptr);
  LineTo(hdc, w, h / 2);
  MoveToEx(hdc, w / 2, 0, nullptr);
  LineTo(hdc, w / 2, h);
  HBRUSH land = CreateSolidBrush(RGB(46, 92, 64));
  RECT land_rc{w / 5, h / 4, w * 4 / 5, h * 3 / 4};
  FillRect(hdc, &land_rc, land);
  DeleteObject(land);
  SelectObject(hdc, old);
  DeleteObject(grid);
  DeleteObject(axis);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(230, 236, 242));
  const wchar_t* title = oop_
                             ? L"Native map HWND — waiting for OOP frame"
                             : L"Native map HWND — local fallback (not WebGL)";
  TextOutW(hdc, 12, 10, title, static_cast<int>(wcslen(title)));
  if (probe_ && probe_[0]) {
    TextOutW(hdc, 12, 32, probe_, static_cast<int>(wcslen(probe_)));
  }
  wchar_t ext[160];
  swprintf_s(ext, L"Extent %.2f, %.2f — %.2f, %.2f  (drag/wheel)",
             local_extent_.xmin, local_extent_.ymin, local_extent_.xmax,
             local_extent_.ymax);
  TextOutW(hdc, 12, h > 60 ? h - 24 : 54, ext, static_cast<int>(wcslen(ext)));
}

void MapPane::dispatch_mouse(UINT msg, WPARAM wp, LPARAM lp) {
  content::InputEvent e = {};
  e.t_qpc = qpc_now();
  e.flags = static_cast<uint32_t>(wp);
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lp)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lp)));
  if (msg == WM_MOUSEWHEEL) {
    POINT pt{e.x_px, e.y_px};
    ScreenToClient(hwnd_, &pt);
    e.x_px = pt.x;
    e.y_px = pt.y;
    e.wheel = GET_WHEEL_DELTA_WPARAM(wp);
    e.kind = content::InputEvent::Kind::kWheel;
  } else if (msg == WM_MOUSEMOVE) {
    e.kind = content::InputEvent::Kind::kMouseMove;
  } else if (msg == WM_LBUTTONDOWN) {
    e.kind = content::InputEvent::Kind::kLDown;
  } else if (msg == WM_LBUTTONUP) {
    e.kind = content::InputEvent::Kind::kLUp;
  } else if (msg == WM_LBUTTONDBLCLK) {
    e.kind = content::InputEvent::Kind::kLDClick;
  } else if (msg == WM_RBUTTONDOWN) {
    e.kind = content::InputEvent::Kind::kRDown;
  } else if (msg == WM_RBUTTONUP) {
    e.kind = content::InputEvent::Kind::kRUp;
  } else if (msg == WM_RBUTTONDBLCLK) {
    e.kind = content::InputEvent::Kind::kRDClick;
  } else {
    e.kind = content::InputEvent::Kind::kKeyDown;
    e.key = static_cast<uint32_t>(wp);
  }

  if (session_ && session_->IsOopRender()) {
    session_->Dispatch(view_id_, e);
    return;
  }

  RECT rc;
  GetClientRect(hwnd_, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) {
    return;
  }
  if (e.kind == content::InputEvent::Kind::kLDown) {
    dragging_ = true;
    last_x_ = e.x_px;
    last_y_ = e.y_px;
    SetCapture(hwnd_);
  } else if (e.kind == content::InputEvent::Kind::kLUp) {
    dragging_ = false;
    ReleaseCapture();
  } else if (e.kind == content::InputEvent::Kind::kMouseMove && dragging_) {
    const double gw = local_extent_.xmax - local_extent_.xmin;
    const double gh = local_extent_.ymax - local_extent_.ymin;
    local_extent_.xmin -= (e.x_px - last_x_) * gw / w;
    local_extent_.xmax -= (e.x_px - last_x_) * gw / w;
    local_extent_.ymin += (e.y_px - last_y_) * gh / h;
    local_extent_.ymax += (e.y_px - last_y_) * gh / h;
    last_x_ = e.x_px;
    last_y_ = e.y_px;
    invalidate();
  } else if (e.kind == content::InputEvent::Kind::kWheel && e.wheel != 0) {
    const double factor = e.wheel > 0 ? 0.8 : 1.25;
    const double fx =
        local_extent_.xmin +
        (local_extent_.xmax - local_extent_.xmin) * (e.x_px / (double)w);
    const double fy =
        local_extent_.ymax -
        (local_extent_.ymax - local_extent_.ymin) * (e.y_px / (double)h);
    local_extent_.xmin = fx + (local_extent_.xmin - fx) * factor;
    local_extent_.xmax = fx + (local_extent_.xmax - fx) * factor;
    local_extent_.ymin = fy + (local_extent_.ymin - fy) * factor;
    local_extent_.ymax = fy + (local_extent_.ymax - fy) * factor;
    invalidate();
  }
}

}  // namespace webview2
}  // namespace app
