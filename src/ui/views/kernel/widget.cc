// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include <windowsx.h>

#include "render/skia/canvas.h"
#include "ui/views/kernel/dialog_host.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/kernel/theme.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

namespace ui {
namespace views {
namespace {

const wchar_t kWidgetClass[] = L"SmartGisViewsWidget";

void collect_focusable(View* v, std::vector<View*>* out) {
  if (!v || !v->is_visible()) {
    return;
  }
  if (v->is_focusable() && v->is_enabled()) {
    out->push_back(v);
  }
  for (size_t i = 0; i < v->child_count(); ++i) {
    collect_focusable(v->child_at(i), out);
  }
}

}  // namespace

Widget::Widget() = default;

Widget::~Widget() {
  destroying_ = true;
  focused_ = nullptr;
  hovered_ = nullptr;
  pressed_ = nullptr;
  release_paint_buffer();
  if (hwnd_ && IsWindow(hwnd_)) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
}

bool Widget::init(const InitParams& params) {
  enable_process_dpi_awareness();

  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = Widget::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // NULL_BRUSH: never flash system COLOR_WINDOW behind Skia chrome.
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = kWidgetClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  if (!registered) {
    return false;
  }
  const DWORD style =
      params.owner ? kOwnedDialogStyle
                   : (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int width = params.width;
  int height = params.height;
  if (params.owner) {
    // Dialog callers pass client size (DIPs when size_in_dips). Shared host
    // scales, expands via AdjustWindowRectEx, centers on owner, clamps work.
    OwnedPopupGeom place;
    if (params.size_in_dips) {
      place = place_owned_dialog(params.owner, params.width, params.height,
                                 style, 0);
    } else {
      const float scale =
          scale_factor_from_dpi(dpi_for_hwnd(params.owner));
      place = place_owned_dialog(params.owner, px_to_dip(params.width, scale),
                                 px_to_dip(params.height, scale), style, 0);
    }
    x = place.x;
    y = place.y;
    width = place.outer_width;
    height = place.outer_height;
  } else if (params.size_in_dips) {
    const float scale = scale_factor_from_dpi(dpi_for_hwnd(nullptr));
    width = dip_to_px(params.width, scale);
    height = dip_to_px(params.height, scale);
    if (width < 160) {
      width = 160;
    }
    if (height < 120) {
      height = 120;
    }
  } else {
    if (width < 160) {
      width = 160;
    }
    if (height < 120) {
      height = 120;
    }
  }
  hwnd_ = CreateWindowExW(0, kWidgetClass, params.title, style, x, y, width,
                          height, params.owner, nullptr, GetModuleHandleW(nullptr),
                          this);
  if (!hwnd_) {
    return false;
  }
  // Owned popups stay above the owner without stealing the taskbar slot.
  if (params.owner && IsWindow(params.owner)) {
    SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  sync_dpi_from_hwnd();
  return true;
}

void Widget::set_contents_view(std::unique_ptr<View> contents) {
  focused_ = nullptr;
  hovered_ = nullptr;
  pressed_ = nullptr;
  contents_ = std::move(contents);
  if (contents_) {
    contents_->set_widget(this);
    if (device_scale_factor_ != 1.f) {
      contents_->propagate_device_scale_factor_changed(1.f, device_scale_factor_);
    }
    layout_contents();
    contents_->realize_native_tree();
  }
}

void Widget::set_device_scale_factor(float scale_factor) {
  if (scale_factor <= 0.f) {
    return;
  }
  const float old = device_scale_factor_;
  if (std::fabs(old - scale_factor) < 0.0001f) {
    return;
  }
  device_scale_factor_ = scale_factor;
  dpi_ = static_cast<unsigned>(
      std::lround(scale_factor * static_cast<float>(kDefaultDpi)));
  if (contents_) {
    contents_->propagate_device_scale_factor_changed(old, device_scale_factor_);
    layout_contents();
    schedule_paint();
  }
}

void Widget::sync_dpi_from_hwnd() {
  dpi_ = dpi_for_hwnd(hwnd_);
  device_scale_factor_ = scale_factor_from_dpi(dpi_);
}

void Widget::on_dpi_changed(unsigned new_dpi, const RECT* suggested) {
  if (new_dpi == 0) {
    return;
  }
  const float old_scale = device_scale_factor_;
  const float new_scale = scale_factor_from_dpi(new_dpi);
  dpi_ = new_dpi;
  device_scale_factor_ = new_scale;
  if (suggested && hwnd_) {
    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left,
                 suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
  if (contents_ && old_scale > 0.f &&
      std::fabs(old_scale - new_scale) >= 0.0001f) {
    contents_->propagate_device_scale_factor_changed(old_scale, new_scale);
  }
  layout_contents();
  schedule_paint();
}

void Widget::show() {
  if (hwnd_) {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
  }
}

int Widget::run_loop() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

int Widget::run_modal() {
  if (!hwnd_) {
    return 0;
  }
  modal_ = true;
  show();
  MSG msg;
  while (hwnd_ && IsWindow(hwnd_)) {
    const BOOL ok = GetMessageW(&msg, nullptr, 0, 0);
    if (ok <= 0) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  modal_ = false;
  return 0;
}

void Widget::request_close() {
  if (!hwnd_ || !IsWindow(hwnd_)) {
    hwnd_ = nullptr;
    return;
  }
  destroying_ = true;
  DestroyWindow(hwnd_);
}

void Widget::layout_contents() {
  if (!contents_ || !hwnd_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  contents_->set_bounds({0, 0, rc.right - rc.left, rc.bottom - rc.top});
  contents_->layout();
  contents_->realize_native_tree();
}

void Widget::schedule_paint() {
  if (hwnd_ && IsWindow(hwnd_)) {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void Widget::schedule_paint_rect(const Rect& dirty) {
  if (!hwnd_ || !IsWindow(hwnd_)) {
    return;
  }
  if (dirty.width <= 0 || dirty.height <= 0) {
    return;
  }
  RECT rc = {dirty.x, dirty.y, dirty.x + dirty.width, dirty.y + dirty.height};
  InvalidateRect(hwnd_, &rc, FALSE);
}

void Widget::release_paint_buffer() {
  if (paint_dc_) {
    if (paint_old_) {
      SelectObject(paint_dc_, paint_old_);
      paint_old_ = nullptr;
    }
    DeleteDC(paint_dc_);
    paint_dc_ = nullptr;
  }
  if (paint_dib_) {
    DeleteObject(paint_dib_);
    paint_dib_ = nullptr;
  }
  paint_w_ = 0;
  paint_h_ = 0;
}

bool Widget::ensure_paint_buffer(int width_px, int height_px) {
  if (width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (paint_dc_ && paint_dib_ && paint_w_ == width_px &&
      paint_h_ == height_px) {
    return true;
  }
  release_paint_buffer();

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;
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
  paint_old_ = static_cast<HBITMAP>(SelectObject(mem, dib));
  paint_dc_ = mem;
  paint_dib_ = dib;
  paint_w_ = width_px;
  paint_h_ = height_px;
  return true;
}

void Widget::set_focused_view(View* view) {
  if (focused_ == view) {
    return;
  }
  View* previous = focused_;
  focused_ = view;
  if (previous) {
    previous->on_blur();
  }
  if (focused_) {
    focused_->on_focus();
  }
  schedule_paint();
}

void Widget::clear_view_refs(View* view) {
  if (focused_ == view) {
    focused_ = nullptr;
  }
  if (hovered_ == view) {
    hovered_ = nullptr;
  }
  if (pressed_ == view) {
    pressed_ = nullptr;
  }
}

bool Widget::advance_focus(bool reverse) {
  if (!contents_) {
    return false;
  }
  std::vector<View*> order;
  collect_focusable(contents_.get(), &order);
  if (order.empty()) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < order.size(); ++i) {
    if (order[i] == focused_) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (reverse) {
    index = (index <= 0) ? static_cast<int>(order.size()) - 1 : index - 1;
  } else {
    index = (index + 1) % static_cast<int>(order.size());
  }
  set_focused_view(order[static_cast<size_t>(index)]);
  return true;
}

bool Widget::send_mouse(const MouseEvent& event) {
  if (!contents_) {
    return false;
  }
  View* hit = contents_->get_view_at(event.x, event.y);
  if (event.type == MouseEvent::Type::kMove) {
    update_hover(hit);
  }
  if (event.type == MouseEvent::Type::kDown && event.button == 1) {
    if (pressed_) {
      pressed_->set_pressed(false);
    }
    pressed_ = hit;
    if (pressed_) {
      pressed_->set_pressed(true);
    }
    if (hit && hit->is_focusable() && hit->is_enabled()) {
      hit->request_focus();
    } else {
      set_focused_view(nullptr);
    }
  }
  bool handled = false;
  // Keep move/up on the press target so Splitter drag survives leaving the bar.
  if (pressed_ &&
      (event.type == MouseEvent::Type::kMove ||
       (event.type == MouseEvent::Type::kUp && event.button == 1))) {
    handled = pressed_->on_mouse_event(event);
  } else {
    handled = contents_->on_mouse_event(event);
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
    if (pressed_) {
      pressed_->set_pressed(false);
      pressed_ = nullptr;
    }
  }
  return handled;
}

bool Widget::send_key(const KeyEvent& event) {
  if (!contents_) {
    return false;
  }
  if (event.type == KeyEvent::Type::kDown && event.vk == VK_TAB) {
    const bool reverse = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    return advance_focus(reverse);
  }
  if (focused_) {
    return focused_->on_key_event(event);
  }
  return contents_->on_key_event(event);
}

bool Widget::send_char(const CharEvent& event) {
  if (!contents_) {
    return false;
  }
  if (event.ch == L'\t' || event.ch == L'\r' || event.ch == L'\n') {
    return false;
  }
  if (focused_) {
    return focused_->on_char_event(event);
  }
  return contents_->on_char_event(event);
}

Widget* Widget::from_hwnd(HWND hwnd) {
  return reinterpret_cast<Widget*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK Widget::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Widget*>(cs->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  Widget* self = from_hwnd(hwnd);
  if (!self) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return self->handle_message(hwnd, msg, wparam, lparam);
}

LRESULT Widget::handle_message(HWND hwnd, UINT msg, WPARAM wparam,
                               LPARAM lparam) {
  switch (msg) {
    case WM_PAINT:
      on_paint();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      on_size(LOWORD(lparam), HIWORD(lparam));
      return 0;
    case WM_GETDPISCALEDSIZE: {
      auto* size = reinterpret_cast<SIZE*>(lparam);
      const unsigned new_dpi = static_cast<unsigned>(wparam);
      if (!size || dpi_ == 0 || new_dpi == 0) {
        break;
      }
      RECT wr = {};
      GetWindowRect(hwnd, &wr);
      const float ratio =
          static_cast<float>(new_dpi) / static_cast<float>(dpi_);
      size->cx = static_cast<LONG>(
          std::lround((wr.right - wr.left) * static_cast<double>(ratio)));
      size->cy = static_cast<LONG>(
          std::lround((wr.bottom - wr.top) * static_cast<double>(ratio)));
      return TRUE;
    }
    case WM_DPICHANGED: {
      const unsigned new_dpi = static_cast<unsigned>(HIWORD(wparam));
      const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
      on_dpi_changed(new_dpi, suggested);
      return 0;
    }
    case WM_LBUTTONDOWN:
      SetCapture(hwnd_);
      dispatch_mouse(MouseEvent::Type::kDown, wparam, lparam, 1, 0);
      return 0;
    case WM_LBUTTONUP:
      if (GetCapture() == hwnd_) {
        ReleaseCapture();
      }
      dispatch_mouse(MouseEvent::Type::kUp, wparam, lparam, 1, 0);
      return 0;
    case WM_LBUTTONDBLCLK:
      dispatch_mouse(MouseEvent::Type::kDblClick, wparam, lparam, 1, 0);
      return 0;
    case WM_RBUTTONDOWN:
      dispatch_mouse(MouseEvent::Type::kDown, wparam, lparam, 2, 0);
      return 0;
    case WM_RBUTTONUP:
      dispatch_mouse(MouseEvent::Type::kUp, wparam, lparam, 2, 0);
      return 0;
    case WM_MOUSEMOVE:
      track_mouse_leave();
      dispatch_mouse(MouseEvent::Type::kMove, wparam, lparam, 0, 0);
      return 0;
    case WM_MOUSELEAVE:
      tracking_leave_ = false;
      update_hover(nullptr);
      return 0;
    case WM_MOUSEWHEEL:
      dispatch_mouse(MouseEvent::Type::kWheel, wparam, lparam, 0,
                     GET_WHEEL_DELTA_WPARAM(wparam));
      return 0;
    case WM_KEYDOWN:
      if (modal_ && wparam == VK_ESCAPE) {
        request_close();
        return 0;
      }
      dispatch_key(KeyEvent::Type::kDown, wparam, lparam);
      return 0;
    case WM_KEYUP:
      dispatch_key(KeyEvent::Type::kUp, wparam, lparam);
      return 0;
    case WM_CHAR: {
      CharEvent e;
      e.ch = static_cast<wchar_t>(wparam);
      send_char(e);
      return 0;
    }
    case WM_CLOSE:
      if (modal_) {
        request_close();
        return 0;
      }
      break;
    case WM_DESTROY:
      // User close (Alt+F4 / X) ends run_loop. ~Widget DestroyWindow must
      // not PostQuitMessage: that poisons a console test thread.
      // Modal dialogs must also skip it so the owner message loop stays alive.
      if (!destroying_ && !modal_) {
        PostQuitMessage(0);
      }
      return 0;
    case WM_NCDESTROY:
      hwnd_ = nullptr;
      return DefWindowProcW(hwnd, msg, wparam, lparam);
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void Widget::on_paint() {
  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd_, &ps);
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) {
    EndPaint(hwnd_, &ps);
    return;
  }

  HDC target = hdc;
  if (ensure_paint_buffer(w, h)) {
    target = paint_dc_;
  }

  const int font_px = dip_to_px(12, device_scale_factor_);
  HFONT font =
      CreateFontW(-font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(target, font) : nullptr;
  // Skia Canvas BitBlts each primitive into |target|. When |target| is the
  // retained buffer, the screen only updates on the final BitBlt below —
  // otherwise mouse-hover full invalidates flash chrome mid-paint.
  render::skia::Canvas canvas(target, w, h);
  canvas.fill_rect(0, 0, w, h, Theme::current().chrome_bg);
  if (contents_) {
    contents_->paint(&canvas);
  }
  if (font) {
    SelectObject(target, old_font);
    DeleteObject(font);
  }

  if (target != hdc && paint_dc_) {
    const int blt_x = ps.rcPaint.left;
    const int blt_y = ps.rcPaint.top;
    const int blt_w = ps.rcPaint.right - ps.rcPaint.left;
    const int blt_h = ps.rcPaint.bottom - ps.rcPaint.top;
    if (blt_w > 0 && blt_h > 0) {
      BitBlt(hdc, blt_x, blt_y, blt_w, blt_h, paint_dc_, blt_x, blt_y,
             SRCCOPY);
    }
  }
  EndPaint(hwnd_, &ps);
}

void Widget::on_size(int, int) {
  layout_contents();
}

bool Widget::dispatch_mouse(MouseEvent::Type type, WPARAM wparam, LPARAM lparam,
                            int button, int wheel) {
  MouseEvent e;
  e.type = type;
  e.flags = static_cast<int>(wparam);
  e.button = button;
  e.wheel_delta = wheel;
  if (type == MouseEvent::Type::kWheel) {
    POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(hwnd_, &pt);
    e.x = pt.x;
    e.y = pt.y;
  } else {
    e.x = GET_X_LPARAM(lparam);
    e.y = GET_Y_LPARAM(lparam);
  }
  return send_mouse(e);
}

bool Widget::dispatch_key(KeyEvent::Type type, WPARAM wparam, LPARAM lparam) {
  KeyEvent e;
  e.type = type;
  e.vk = static_cast<std::uint32_t>(wparam);
  e.flags = static_cast<int>(lparam);
  return send_key(e);
}

void Widget::track_mouse_leave() {
  if (tracking_leave_ || !hwnd_) {
    return;
  }
  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(tme);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd_;
  tracking_leave_ = TrackMouseEvent(&tme) != FALSE;
}

void Widget::update_hover(View* hit) {
  if (hovered_ == hit) {
    return;
  }
  if (hovered_) {
    hovered_->set_hovered(false);
  }
  hovered_ = hit;
  if (hovered_) {
    hovered_->set_hovered(true);
  }
}

}  // namespace views
}  // namespace ui
