// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/widget.h"

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <vector>

#include <windowsx.h>

#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dialog_host.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/paint/register_default_painters.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/compositor/shell_compositor.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

namespace ui {
namespace views {

class WidgetThemeWatch : public ThemeObserver {
 public:
  explicit WidgetThemeWatch(Widget* widget) : widget_(widget) {}
  void on_theme_changed() override {
    if (widget_) {
      widget_->on_theme_changed();
    }
  }

 private:
  Widget* widget_ = nullptr;
};

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

Widget::Widget() {}

Widget::~Widget() {
  if (theme_watch_) {
    ThemeService::get().remove_observer(theme_watch_.get());
    theme_watch_.reset();
  }
  destroying_ = true;
  will_close_fired_ = true;
  will_close_.reset();
  focused_ = nullptr;
  hovered_ = nullptr;
  pressed_ = nullptr;
  // Join the raster worker before tearing down the View tree / HWND.
  shutdown_compositor();
  contents_.reset();
  if (hwnd_ && IsWindow(hwnd_)) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
  }
  hwnd_ = nullptr;
}

bool Widget::init(const InitParams& params) {
  enable_process_dpi_awareness();
  ThemeService::get().ensure_builtin_packs();
  ThemeService::get().load_persisted();
  register_default_painters();

  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = Widget::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // NULL_BRUSH: never flash system COLOR_WINDOW behind Skia shell.
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = kWidgetClass;
    registered = RegisterClassExW(&wc) != 0;
  }
  if (!registered) {
    return false;
  }

  frame_kind_ = params.frame_kind;
  const bool custom = frame_kind_ == FrameKind::kCustom;
  // Custom frame must not include WS_CAPTION: DWM would still paint the OS
  // title bar even when WM_NCCALCSIZE collapses NC, causing a double caption
  // (white system bar + FrameView). Match owned dialogs: WS_POPUP + thickframe.
  // Top-level adds min/max boxes; WS_EX_APPWINDOW keeps a taskbar button.
  const DWORD style =
      custom
          ? (WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_CLIPCHILDREN |
             (params.owner ? 0u : (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)))
          : (params.owner ? kOwnedDialogStyle
                          : (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN));
  const DWORD ex_style =
      (custom && !params.owner) ? WS_EX_APPWINDOW : 0u;
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int width = params.width;
  int height = params.height;
  if (params.owner) {
    if (custom) {
      float scale = scale_factor_from_dpi(dpi_for_hwnd(params.owner));
      int client_w = params.size_in_dips ? dip_to_px(params.width, scale)
                                         : params.width;
      int client_h = params.size_in_dips ? dip_to_px(params.height, scale)
                                         : params.height;
      // Custom frame: CreateWindow size == client (WM_NCCALCSIZE collapses NC).
      // Add caption height into the requested client so dialogs keep body size.
      client_h += dip_to_px(32, scale);
      RECT owner_rc = {};
      if (IsWindow(params.owner)) {
        GetWindowRect(params.owner, &owner_rc);
      }
      OwnedPopupGeom place =
          center_outer_on_owner_rect(owner_rc, client_w, client_h);
      x = place.x;
      y = place.y;
      width = place.outer_width;
      height = place.outer_height;
    } else {
      // Dialog callers pass client size (DIPs when size_in_dips). Shared host
      // scales, expands via AdjustWindowRectEx, centers on owner, clamps work.
      OwnedPopupGeom place;
      if (params.size_in_dips) {
        place = place_owned_dialog(params.owner, params.width, params.height,
                                   style, 0);
      } else {
        const float scale =
            scale_factor_from_dpi(dpi_for_hwnd(params.owner));
        place =
            place_owned_dialog(params.owner, px_to_dip(params.width, scale),
                               px_to_dip(params.height, scale), style, 0);
      }
      x = place.x;
      y = place.y;
      width = place.outer_width;
      height = place.outer_height;
    }
  } else {
    // Top-level shell: InitParams is *client* size. Scale DIPs, expand to
    // outer CreateWindow box (system frame) or keep as-is (custom CSD).
    int client_w = params.width;
    int client_h = params.height;
    if (params.size_in_dips) {
      const float scale = scale_factor_from_dpi(dpi_for_hwnd(nullptr));
      client_w = dip_to_px(params.width, scale);
      client_h = dip_to_px(params.height, scale);
    }
    if (client_w < 160) {
      client_w = 160;
    }
    if (client_h < 120) {
      client_h = 120;
    }
    if (custom) {
      width = client_w;
      height = client_h;
    } else {
      client_to_outer_size(client_w, client_h, style, 0, &width, &height);
    }
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    POINT origin = {};
    HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    if (mon && GetMonitorInfoW(mon, &mi)) {
      const int work_w = mi.rcWork.right - mi.rcWork.left;
      const int work_h = mi.rcWork.bottom - mi.rcWork.top;
      if (work_w > 0 && width > work_w) {
        width = work_w;
      }
      if (work_h > 0 && height > work_h) {
        height = work_h;
      }
    }
  }
  hwnd_ = CreateWindowExW(ex_style, kWidgetClass, params.title, style, x, y,
                          width, height, params.owner, nullptr,
                          GetModuleHandleW(nullptr), this);
  if (!hwnd_) {
    return false;
  }
  // Owned popups stay above the owner without stealing the taskbar slot.
  if (params.owner && IsWindow(params.owner)) {
    SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  sync_dpi_from_hwnd();
  theme_watch_ = std::make_unique<WidgetThemeWatch>(this);
  ThemeService::get().add_observer(theme_watch_.get());
  compositor_ = std::make_unique<ShellCompositor>();
  compositor_->start();
  return true;
}

ui::gfx::ShellRaster Widget::shell_raster() const {
  if (!compositor_) {
    return {};
  }
  return compositor_->shell_raster();
}

std::uint64_t Widget::shell_generation() const {
  if (!compositor_) {
    return 0;
  }
  return compositor_->published_generation();
}

void Widget::shutdown_compositor() {
  if (!compositor_) {
    return;
  }
  compositor_->shutdown();
  compositor_.reset();
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
  fire_will_close();
  destroying_ = true;
  DestroyWindow(hwnd_);
}

void Widget::set_will_close(WillClose fn) {
  if (!fn) {
    will_close_.reset();
    return;
  }
  will_close_ = std::make_unique<WillClose>(std::move(fn));
}

void Widget::set_on_shell_published(OnShellPublished fn) {
  if (!fn) {
    on_shell_published_.reset();
    return;
  }
  on_shell_published_ = std::make_unique<OnShellPublished>(std::move(fn));
}

void Widget::fire_will_close() {
  if (will_close_fired_) {
    return;
  }
  will_close_fired_ = true;
  // Stop feeding MapViewport shell overlays before GPU / HWND teardown.
  on_shell_published_.reset();
  shell_wake_invalidate_pending_ = false;
  awaiting_publish_gen_ = 0;
  awaiting_publish_dirty_ = {};
  last_shell_published_notified_ = 0;
  // Spec order: host will_close stops BeginFrame + drains GPU (detach), then
  // join the shell raster worker (clears notify_when_published HWND).
  // DestroyWindow stays after this returns — late kShellPublishedMessage is
  // ignored via will_close_fired_ / destroying_.
  if (will_close_ && *will_close_) {
    WillClose fn = std::move(*will_close_);
    will_close_.reset();
    fn();
  } else {
    will_close_.reset();
  }
  shutdown_compositor();
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

void Widget::union_pending_dirty(const Rect& dirty) {
  if (dirty.width <= 0 || dirty.height <= 0) {
    return;
  }
  if (full_paint_pending_ || pending_dirty_.width <= 0 ||
      pending_dirty_.height <= 0) {
    pending_dirty_ = dirty;
    return;
  }
  const int l = dirty.x < pending_dirty_.x ? dirty.x : pending_dirty_.x;
  const int t = dirty.y < pending_dirty_.y ? dirty.y : pending_dirty_.y;
  const int r = dirty.right() > pending_dirty_.right() ? dirty.right()
                                                       : pending_dirty_.right();
  const int b = dirty.bottom() > pending_dirty_.bottom()
                    ? dirty.bottom()
                    : pending_dirty_.bottom();
  pending_dirty_ = Rect{l, t, r - l, b - t};
}

void Widget::take_pending_dirty(int width_px, int height_px, Rect* out) {
  if (!out) {
    return;
  }
  if (full_paint_pending_ || pending_dirty_.width <= 0 ||
      pending_dirty_.height <= 0) {
    *out = Rect{0, 0, width_px, height_px};
  } else {
    *out = pending_dirty_;
  }
  pending_dirty_ = {};
  full_paint_pending_ = false;
}

bool Widget::has_pending_paint() const {
  return full_paint_pending_ ||
         (pending_dirty_.width > 0 && pending_dirty_.height > 0);
}

void Widget::schedule_paint() {
  full_paint_pending_ = true;
  pending_dirty_ = {};
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
  if (!full_paint_pending_) {
    union_pending_dirty(dirty);
  }
  RECT rc = {dirty.x, dirty.y, dirty.x + dirty.width, dirty.y + dirty.height};
  InvalidateRect(hwnd_, &rc, FALSE);
}

void Widget::set_focused_view(View* view) {
  if (focused_ == view) {
    return;
  }
  View* previous = focused_;
  focused_ = view;
  if (previous) {
    previous->on_blur();
    previous->invalidate_commands();
  }
  if (focused_) {
    focused_->on_focus();
    focused_->invalidate_commands();
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

void Widget::on_theme_changed() {
  if (contents_) {
    std::function<void(View*)> dirty = [&](View* v) {
      if (!v) {
        return;
      }
      v->invalidate_commands();
      for (size_t i = 0; i < v->child_count(); ++i) {
        dirty(v->child_at(i));
      }
    };
    dirty(contents_.get());
  }
  schedule_paint();
}

LRESULT Widget::handle_nc_hit_test(int screen_x, int screen_y) {
  if (!hwnd_) {
    return HTCLIENT;
  }
  POINT pt = {screen_x, screen_y};
  ScreenToClient(hwnd_, &pt);
  RECT cr = {};
  GetClientRect(hwnd_, &cr);
  const int w = cr.right - cr.left;
  const int h = cr.bottom - cr.top;
  const int border = (std::max)(1, dip_to_px(6, device_scale_factor_));

  WINDOWPLACEMENT wp = {};
  wp.length = sizeof(wp);
  GetWindowPlacement(hwnd_, &wp);
  const bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

  if (!maximized) {
    const bool left = pt.x < border;
    const bool right = pt.x >= w - border;
    const bool top = pt.y < border;
    const bool bottom = pt.y >= h - border;
    if (top && left) {
      return HTTOPLEFT;
    }
    if (top && right) {
      return HTTOPRIGHT;
    }
    if (bottom && left) {
      return HTBOTTOMLEFT;
    }
    if (bottom && right) {
      return HTBOTTOMRIGHT;
    }
    if (left) {
      return HTLEFT;
    }
    if (right) {
      return HTRIGHT;
    }
    if (top) {
      return HTTOP;
    }
    if (bottom) {
      return HTBOTTOM;
    }
  }

  auto* frame = dynamic_cast<FrameView*>(contents_.get());
  const int caption_h = frame ? frame->caption_height_px()
                              : dip_to_px(32, device_scale_factor_);
  if (pt.y >= 0 && pt.y < caption_h) {
    if (frame && frame->point_in_caption_controls(pt.x, pt.y)) {
      return HTCLIENT;
    }
    return HTCAPTION;
  }
  return HTCLIENT;
}

LRESULT Widget::handle_message(HWND hwnd, UINT msg, WPARAM wparam,
                               LPARAM lparam) {
  switch (msg) {
    case WM_NCCALCSIZE:
      if (frame_kind_ == FrameKind::kCustom) {
        // Client area fills the entire window; FrameView paints the caption.
        // Handle both wParam TRUE and FALSE — falling through to DefWindowProc
        // would re-apply WS_CAPTION insets and show a second OS title bar.
        return 0;
      }
      break;
    case WM_NCHITTEST:
      if (frame_kind_ == FrameKind::kCustom) {
        return handle_nc_hit_test(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
      }
      break;
    case WM_NCLBUTTONDBLCLK:
      if (frame_kind_ == FrameKind::kCustom && wparam == HTCAPTION) {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);
        ShowWindow(hwnd, (wp.showCmd == SW_SHOWMAXIMIZED) ? SW_RESTORE
                                                          : SW_MAXIMIZE);
        if (auto* frame = dynamic_cast<FrameView*>(contents_.get())) {
          frame->sync_maximize_button(wp.showCmd != SW_SHOWMAXIMIZED);
        }
        return 0;
      }
      break;
    case WM_SIZE:
      if (frame_kind_ == FrameKind::kCustom) {
        if (auto* frame = dynamic_cast<FrameView*>(contents_.get())) {
          frame->sync_maximize_button(wparam == SIZE_MAXIMIZED);
        }
      }
      on_size(LOWORD(lparam), HIWORD(lparam));
      return 0;
    case WM_PAINT:
      on_paint();
      return 0;
    case kShellPublishedMessage:
      on_shell_published_message(static_cast<std::uint64_t>(wparam));
      return 0;
    case WM_ERASEBKGND:
      return 1;
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
      fire_will_close();
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

void Widget::on_shell_published_message(std::uint64_t generation) {
  // Worker wake: coalesce InvalidateRect on the UI thread. Do not Commit or
  // Present here — the next WM_PAINT BitBlts the published front.
  if (destroying_ || will_close_fired_ || !hwnd_ || !IsWindow(hwnd_)) {
    return;
  }
  if (generation == 0 || generation < awaiting_publish_gen_) {
    return;
  }
  // Do not drop this wake. A WM_PAINT already inside BeginPaint may BitBlt the
  // previous front and clear the coalesce flag, so swallowing the publish
  // leaves the new rect stale until a hover paint.
  shell_wake_invalidate_pending_ = true;
  if (awaiting_publish_dirty_.width > 0 && awaiting_publish_dirty_.height > 0) {
    RECT rc = {awaiting_publish_dirty_.x, awaiting_publish_dirty_.y,
               awaiting_publish_dirty_.x + awaiting_publish_dirty_.width,
               awaiting_publish_dirty_.y + awaiting_publish_dirty_.height};
    InvalidateRect(hwnd_, &rc, FALSE);
  } else {
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void Widget::maybe_notify_shell_published(std::uint64_t published_gen) {
  if (!on_shell_published_ || !*on_shell_published_) {
    return;
  }
  if (published_gen == 0 || published_gen < awaiting_publish_gen_ ||
      published_gen == last_shell_published_notified_) {
    return;
  }
  last_shell_published_notified_ = published_gen;
  (*on_shell_published_)(awaiting_publish_dirty_);
}

void Widget::on_paint() {
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd_, &ps);
  shell_wake_invalidate_pending_ = false;
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0 || IsIconic(hwnd_) || !compositor_) {
    if (compositor_ && (w <= 0 || h <= 0 || IsIconic(hwnd_))) {
      compositor_->release_buffers();
    }
    EndPaint(hwnd_, &ps);
    return;
  }

  if (contents_ && contents_->needs_layout()) {
    layout_contents();
  }

  // UI thread: record + Commit immutable DisplayList when chrome dirtied the
  // HWND. Do NOT wait_published — that blocked hover on every WM_PAINT.
  // Present the last published front immediately; the worker posts
  // kShellPublishedMessage when a newer generation is ready, and the next
  // paint BitBlts it. Wake-only paints (no pending dirty) skip Commit so a
  // publish notify cannot re-record the whole tree.
  Rect committed_dirty{};
  bool did_commit = false;
  if (contents_ && has_pending_paint()) {
    take_pending_dirty(w, h, &committed_dirty);
    PaintCommit frame;
    const int font_px = dip_to_px(12, device_scale_factor_);
    if (commit_view_tree(contents_.get(), committed_dirty, w, h, font_px,
                         Theme::current().shell_bg, &frame)) {
      awaiting_publish_gen_ = frame.generation;
      awaiting_publish_dirty_ = committed_dirty;
      compositor_->commit(std::move(frame));
      compositor_->notify_when_published(awaiting_publish_gen_, hwnd_);
      did_commit = true;
    }
  }

  // Layout / set_bounds may expand dirty beyond BeginPaint's update region.
  // BitBlt the union so newly exposed chrome is not left blank until hover.
  RECT blit = ps.rcPaint;
  if (did_commit && committed_dirty.width > 0 && committed_dirty.height > 0) {
    const int l =
        committed_dirty.x < blit.left ? committed_dirty.x : blit.left;
    const int t =
        committed_dirty.y < blit.top ? committed_dirty.y : blit.top;
    const int r = committed_dirty.right() > blit.right ? committed_dirty.right()
                                                       : blit.right;
    const int b = committed_dirty.bottom() > blit.bottom
                      ? committed_dirty.bottom()
                      : blit.bottom;
    blit = {l, t, r, b};
  }
  const std::uint64_t presented_gen =
      compositor_->present(hdc, blit, Theme::current().shell_bg);
  EndPaint(hwnd_, &ps);

  // Commit is async: union blit may still be the previous front. Force a
  // follow-up paint for any committed area outside the original update rect
  // so wake/hover is not the only path that refreshes newly exposed chrome.
  if (did_commit && committed_dirty.width > 0 && committed_dirty.height > 0) {
    const bool expands =
        committed_dirty.x < ps.rcPaint.left ||
        committed_dirty.y < ps.rcPaint.top ||
        committed_dirty.right() > ps.rcPaint.right ||
        committed_dirty.bottom() > ps.rcPaint.bottom;
    if (expands) {
      RECT rc = {committed_dirty.x, committed_dirty.y, committed_dirty.right(),
                 committed_dirty.bottom()};
      InvalidateRect(hwnd_, &rc, FALSE);
    }
  }

  maybe_notify_shell_published(presented_gen);

  const int area_w = ps.rcPaint.right - ps.rcPaint.left;
  const int area_h = ps.rcPaint.bottom - ps.rcPaint.top;
  ui::gfx::note_paint_area(
      static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h),
      area_w > 0 && area_h > 0
          ? static_cast<std::uint64_t>(area_w) *
                static_cast<std::uint64_t>(area_h)
          : 0);
  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  if (t1.QuadPart > t0.QuadPart) {
    ui::gfx::note_widget_paint_qpc(
        static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
  }
}

void Widget::on_size(int width, int height) {
  if (width <= 0 || height <= 0 || (hwnd_ && IsIconic(hwnd_))) {
    if (compositor_) {
      compositor_->release_buffers();
    }
  }
  layout_contents();
  // Force a full shell Commit+Present. Setting full_paint_pending alone is not
  // enough: without InvalidateRect, resize/move can leave WS_CLIPCHILDREN holes
  // and a lagging front DIB unpainted (desktop show-through / overlap).
  schedule_paint();
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
