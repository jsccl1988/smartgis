// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/widget.h"

#include <cstdint>
#include <vector>

#include <windowsx.h>

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

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
  if (hwnd_ && IsWindow(hwnd_)) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
}

bool Widget::init(const InitParams& params) {
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = Widget::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWidgetClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  if (!registered) {
    return false;
  }
  const DWORD style =
      params.owner ? (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                      WS_CLIPCHILDREN)
                   : (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  if (params.owner && IsWindow(params.owner)) {
    RECT rc = {};
    GetWindowRect(params.owner, &rc);
    x = rc.left + (rc.right - rc.left - params.width) / 2;
    y = rc.top + (rc.bottom - rc.top - params.height) / 2;
  }
  hwnd_ = CreateWindowExW(0, kWidgetClass, params.title, style, x, y,
                          params.width, params.height, params.owner, nullptr,
                          GetModuleHandleW(nullptr), this);
  return hwnd_ != nullptr;
}

void Widget::set_contents_view(std::unique_ptr<View> contents) {
  focused_ = nullptr;
  hovered_ = nullptr;
  pressed_ = nullptr;
  contents_ = std::move(contents);
  if (contents_) {
    contents_->set_widget(this);
    layout_contents();
    contents_->realize_native_tree();
  }
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
  render::skia::Canvas canvas(hdc, w, h);
  canvas.fill_rect(0, 0, w, h, Theme::current().chrome_bg);
  if (contents_) {
    contents_->paint(&canvas);
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
