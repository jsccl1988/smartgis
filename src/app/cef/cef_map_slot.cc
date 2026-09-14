// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/cef_map_slot.h"

#include "content/public/map_contents.h"
#include "content/public/map_widget_host_view.h"
#include "content/public/view_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include <cstdio>
#include <cstring>

namespace app {
namespace cef {
namespace {

constexpr wchar_t kChildClass[] = L"SmartGisCefMapSlot";

void register_child_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = CefMapSlot::wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kChildClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

CefMapSlot::CefMapSlot() = default;

CefMapSlot::~CefMapSlot() {
  destroy();
}

bool CefMapSlot::create(HWND parent,
                        content::MapContents* session,
                        content::ViewKind kind) {
  destroy();
  parent_ = parent;
  session_ = session;
  kind_ = kind;
  if (!parent_ || !session_) {
    return false;
  }

  // StartRenderProcess is owned by BrowserMain (once per session).
  render_ok_ = true;
  view_id_ = session_->OpenView(kind_);
  view_ = session_->AttachSurface(view_id_, content::PresentMode::kSoftwareDib);
  view_host_ = new content::ViewHost();

  register_child_class();
  child_hwnd_ = CreateWindowExW(
      0, kChildClass, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, parent_,
      nullptr, GetModuleHandleW(nullptr), this);
  if (!child_hwnd_) {
    return false;
  }
  if (view_) {
    content::MapWidgetHostView::CreateParams params;
    params.parent_hwnd = child_hwnd_;
    view_->Create(params, content::MapWidgetHostView::Preferences());
    view_->SetPresentMode(content::PresentMode::kSoftwareDib);
  }
  start_present_timer();
  set_visible(false);
  return true;
}

void CefMapSlot::destroy() {
  stop_present_timer();
  if (child_hwnd_) {
    DestroyWindow(child_hwnd_);
    child_hwnd_ = nullptr;
  }
  if (session_ && view_id_ != 0) {
    session_->CloseView(view_id_);
    view_id_ = 0;
  }
  view_ = nullptr;
  delete view_host_;
  view_host_ = nullptr;
  session_ = nullptr;
  parent_ = nullptr;
}

void CefMapSlot::sync_layout(const RectPx& rect_px, float dpi) {
  if (!child_hwnd_ || !rect_px.is_valid()) {
    return;
  }
  SetWindowPos(child_hwnd_, HWND_TOP, rect_px.x, rect_px.y, rect_px.w,
               rect_px.h, SWP_NOACTIVATE | (visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
  InvalidateRect(child_hwnd_, nullptr, FALSE);
  if (view_) {
    view_->Resize(rect_px.w, rect_px.h, dpi * 96.f);
  }
}

void CefMapSlot::set_visible(bool visible) {
  visible_ = visible;
  if (!child_hwnd_) {
    return;
  }
  ShowWindow(child_hwnd_, visible ? SW_SHOW : SW_HIDE);
  if (view_) {
    view_->SetVisible(visible);
  }
}

bool CefMapSlot::wait_ready(uint32_t timeout_ms) {
  if (!session_ || view_id_ == 0 || !render_ok_) {
    return false;
  }
  if (!session_->WaitFrameReady(view_id_, timeout_ms)) {
    return false;
  }
  // Require a presentable DIB (not merely a generation bump).
  const DWORD end = GetTickCount() + 2000;
  for (;;) {
    if (has_presented_frame()) {
      return true;
    }
    if (GetTickCount() >= end) {
      return false;
    }
    Sleep(20);
  }
}

bool CefMapSlot::has_presented_frame() const {
  if (!view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px > 0 && surface.height_px > 0;
}

content::ViewHost* CefMapSlot::view_host() {
  return view_host_;
}

void CefMapSlot::start_present_timer() {
  if (child_hwnd_) {
    SetTimer(child_hwnd_, kPresentTimerId, 33, nullptr);
  }
}

void CefMapSlot::stop_present_timer() {
  if (child_hwnd_) {
    KillTimer(child_hwnd_, kPresentTimerId);
  }
}

bool CefMapSlot::present_latest_frame(HDC hdc, const RECT& client_rc) const {
  if (!hdc || !view_) {
    return false;
  }
  const content::SharedSurface surface = view_->Latest();
  if (!surface.nt_handle || surface.generation == 0 || surface.width_px == 0 ||
      surface.height_px == 0) {
    return false;
  }
  const SIZE_T bytes = static_cast<SIZE_T>(surface.width_px) *
                       static_cast<SIZE_T>(surface.height_px) * 4u;
  void* bits =
      MapViewOfFile(static_cast<HANDLE>(surface.nt_handle), FILE_MAP_READ, 0, 0,
                    bytes);
  if (!bits) {
    return false;
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = static_cast<LONG>(surface.width_px);
  bi.bmiHeader.biHeight = -static_cast<LONG>(surface.height_px);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  const int dst_w = client_rc.right > 0 ? client_rc.right : 1;
  const int dst_h = client_rc.bottom > 0 ? client_rc.bottom : 1;
  const int ok =
      StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0,
                    static_cast<int>(surface.width_px),
                    static_cast<int>(surface.height_px), bits, &bi,
                    DIB_RGB_COLORS, SRCCOPY);
  UnmapViewOfFile(bits);
  return ok != 0 && ok != GDI_ERROR;
}

void CefMapSlot::paint_child() const {
  if (!child_hwnd_) {
    return;
  }
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(child_hwnd_, &ps);
  RECT rc;
  GetClientRect(child_hwnd_, &rc);
  if (!present_latest_frame(hdc, rc)) {
    const bool scene3d = kind_ == content::ViewKind::kScene3d;
    const HBRUSH brush =
        CreateSolidBrush(scene3d ? RGB(32, 28, 48) : RGB(28, 42, 58));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(230, 236, 242));
    const wchar_t* line =
        render_ok_ ? L"Map slot (waiting for frame)" : L"Map slot (GPU not started)";
    DrawTextW(hdc, line, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  EndPaint(child_hwnd_, &ps);
}

void CefMapSlot::dispatch_mouse(content::InputEvent::Kind kind,
                                LPARAM lparam,
                                int wheel) {
  if (!view_host_ || !visible_) {
    return;
  }
  content::InputEvent e{};
  e.kind = kind;
  e.x_px = static_cast<int16_t>(LOWORD(lparam));
  e.y_px = static_cast<int16_t>(HIWORD(lparam));
  e.wheel = wheel;
  view_host_->dispatch_input(e);
  if (session_ && view_id_ != 0) {
    session_->Dispatch(view_id_, e);
  }
}

LRESULT CALLBACK CefMapSlot::wnd_proc(HWND hwnd,
                                      UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  CefMapSlot* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = static_cast<CefMapSlot*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<CefMapSlot*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  switch (msg) {
    case WM_PAINT:
      if (self) {
        self->paint_child();
      }
      return 0;
    case WM_TIMER:
      if (self && wparam == kPresentTimerId) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_LBUTTONDOWN:
      if (self) {
        SetCapture(hwnd);
        self->dispatch_mouse(content::InputEvent::Kind::kLDown, lparam, 0);
      }
      return 0;
    case WM_LBUTTONUP:
      if (self) {
        ReleaseCapture();
        self->dispatch_mouse(content::InputEvent::Kind::kLUp, lparam, 0);
      }
      return 0;
    case WM_LBUTTONDBLCLK:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kLDClick, lparam, 0);
      }
      return 0;
    case WM_RBUTTONDOWN:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kRDown, lparam, 0);
      }
      return 0;
    case WM_RBUTTONUP:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kRUp, lparam, 0);
      }
      return 0;
    case WM_MOUSEMOVE:
      if (self) {
        self->dispatch_mouse(content::InputEvent::Kind::kMouseMove, lparam, 0);
      }
      return 0;
    case WM_MOUSEWHEEL:
      if (self) {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &pt);
        const LPARAM client_lp = MAKELPARAM(pt.x, pt.y);
        self->dispatch_mouse(content::InputEvent::Kind::kWheel, client_lp,
                             GET_WHEEL_DELTA_WPARAM(wparam));
      }
      return 0;
    case WM_KEYDOWN:
      if (self && self->view_host_) {
        content::InputEvent e{};
        e.kind = content::InputEvent::Kind::kKeyDown;
        e.key = static_cast<uint32_t>(wparam);
        self->view_host_->dispatch_input(e);
      }
      return 0;
    case WM_NCDESTROY:
      if (self) {
        self->child_hwnd_ = nullptr;
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace cef
}  // namespace app
