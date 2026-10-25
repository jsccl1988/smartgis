// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/widget_lifecycle.h"

#include <cmath>
#include <memory>

#include "base/core/log.h"
#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/paint/register_default_painters.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/kernel/widget/widget_window.h"

namespace ui {
namespace views {
namespace {

// ShowWindow(SW_SHOW) alone often leaves a newly created shell behind the
// previous foreground app (console / IDE / browser). Prefer SetForegroundWindow;
// if Windows blocks focus-steal, briefly TOPMOST + synthetic Alt unlock (do not
// AttachThreadInput — that deadlocks against Unreal/other UI threads).
void bring_widget_to_foreground(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }
  BringWindowToTop(hwnd);
  if (SetForegroundWindow(hwnd)) {
    return;
  }
  // Unlock the foreground lock with a no-op Alt press, then retry.
  INPUT keys[2] = {};
  keys[0].type = INPUT_KEYBOARD;
  keys[0].ki.wVk = VK_MENU;
  keys[1].type = INPUT_KEYBOARD;
  keys[1].ki.wVk = VK_MENU;
  keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, keys, sizeof(INPUT));
  if (SetForegroundWindow(hwnd)) {
    return;
  }
  // Last resort: flash above others without joining another thread's input.
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(hwnd);
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

}  // namespace

// Forwards ThemeService notifications into Widget (friend of Widget).
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

int run_widget_quit_loop() {
  MSG msg = {};
  for (;;) {
    const BOOL ok = GetMessageW(&msg, nullptr, 0, 0);
    if (ok == 0) {
      // WM_QUIT — wParam is the code from PostQuitMessage.
      return static_cast<int>(msg.wParam);
    }
    if (ok < 0) {
      // GetMessage failure must not surface as process exit -1 (0xFFFFFFFF),
      // which looks like a spontaneous crash to launchers / agents.
      const DWORD err = ::GetLastError();
      LOGGING(LOG_ERROR,
              "run_widget_quit_loop GetMessage failed last_error=%lu",
              static_cast<unsigned long>(err));
      return 1;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

int run_widget_modal_loop(HWND hwnd) {
  if (!hwnd) {
    return 0;
  }
  MSG msg;
  while (hwnd && IsWindow(hwnd)) {
    const BOOL ok = GetMessageW(&msg, nullptr, 0, 0);
    if (ok <= 0) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

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

  if (!ensure_widget_window_class(&Widget::wnd_proc)) {
    return false;
  }

  frame_kind_ = params.frame_kind;
  dismiss_on_deactivate_ = params.dismiss_on_deactivate;
  const WidgetWindowCreate create = compute_widget_window_create(params);
  hwnd_ = create_widget_hwnd(create, params, this);
  if (!hwnd_) {
    return false;
  }
  raise_owned_widget_above_owner(hwnd_, params.owner);
  sync_dpi_from_hwnd();
  theme_watch_ = std::make_unique<WidgetThemeWatch>(this);
  ThemeService::get().add_observer(theme_watch_.get());
  compositor_ = std::make_unique<ShellCompositor>();
  return true;
}

void Widget::shutdown_compositor() {
  if (!compositor_) {
    return;
  }
  compositor_->shutdown();
  compositor_.reset();
}

void Widget::show() {
  if (!hwnd_) {
    return;
  }
  ensure_compositor_started();
  ShowWindow(hwnd_, SW_SHOW);
  bring_widget_to_foreground(hwnd_);
  // CreateWindow-time GetDpiForWindow can still report 96 until the HWND is
  // shown on its monitor; without a resync horizon stays at 1.0x until the
  // first hover/scroll paint makes text suddenly jump larger.
  const float old_scale = device_scale_factor_;
  sync_dpi_from_hwnd();
  if (contents_ &&
      std::fabs(old_scale - device_scale_factor_) >= 0.0001f) {
    contents_->propagate_device_scale_factor_changed(old_scale,
                                                   device_scale_factor_);
  }
  // Always layout on show: first ShowWindow often exposes a non-zero client
  // while contents still carry create-time zero bounds (ui-showcase
  // child-outside-parent / status-clipped with parent height 0).
  layout_contents();
  schedule_paint();
  UpdateWindow(hwnd_);
}

int Widget::run_loop() {
  return run_widget_quit_loop();
}

int Widget::run_modal() {
  if (!hwnd_) {
    return 0;
  }
  modal_ = true;
  show();
  const int code = run_widget_modal_loop(hwnd_);
  modal_ = false;
  return code;
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
  will_close_fn_ = nullptr;
  will_close_baton_ = nullptr;
  if (!fn) {
    will_close_.reset();
    return;
  }
  will_close_ = std::make_unique<WillClose>(std::move(fn));
}

void Widget::set_will_close_fn(WillCloseFn fn, void* baton) {
  will_close_.reset();
  will_close_fn_ = fn;
  will_close_baton_ = baton;
}

void Widget::set_on_shell_published(OnShellPublished fn) {
  on_shell_published_fn_ = nullptr;
  on_shell_published_baton_ = nullptr;
  if (!fn) {
    on_shell_published_.reset();
    return;
  }
  on_shell_published_ = std::make_unique<OnShellPublished>(std::move(fn));
}

void Widget::set_on_shell_published_fn(OnShellPublishedFn fn, void* baton) {
  on_shell_published_.reset();
  on_shell_published_fn_ = fn;
  on_shell_published_baton_ = baton;
}

void Widget::fire_will_close() {
  if (will_close_fired_) {
    return;
  }
  will_close_fired_ = true;
  // Stop feeding MapViewport shell overlays before GPU / HWND teardown.
  on_shell_published_.reset();
  on_shell_published_fn_ = nullptr;
  on_shell_published_baton_ = nullptr;
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
  } else if (will_close_fn_) {
    will_close_fn_(will_close_baton_);
    will_close_fn_ = nullptr;
    will_close_baton_ = nullptr;
  } else {
    will_close_.reset();
  }
  shutdown_compositor();
}

}  // namespace views
}  // namespace ui
