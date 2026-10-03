// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_WIDGET_WIDGET_H_
#define UI_VIEWS_KERNEL_WIDGET_WIDGET_H_

#include "ui/ui_export.h"
#include <cstdint>
#include <functional>
#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/gfx/raster/shell_raster.h"

namespace ui {
namespace views {

class ShellCompositor;
class ThemeObserver;

// Top-level native HWND that owns a View tree and dispatches input / paint.
class UI_EXPORT Widget {
 public:
  // kSystem keeps OS non-client chrome. kCustom uses a caption-less HWND
  // (WS_POPUP + thickframe) and collapses NC into the client so FrameView
  // can paint the caption (CSD). Do not combine kCustom with WS_CAPTION —
  // DWM would still draw a second OS title bar.
  // kPopup is a borderless owned dropdown / tip (no caption inflate).
  enum class FrameKind {
    kSystem,
    kCustom,
    kPopup,
  };

  struct InitParams {
    const wchar_t* title = L"SmartGIS Views";
    // Client size. Defaults are DIPs (|size_in_dips| true) so 1280x800 looks
    // like a normal desktop shell on Per-Monitor DPI hosts.
    // Owned popups (|owner| set) always treat this as *client* size; Widget
    // expands to outer shell via dialog_host / AdjustWindowRectEx unless
    // |frame_kind| is kCustom / kPopup (outer == client).
    int width = 1280;
    int height = 800;
    HWND owner = nullptr;
    // When true (default), |width|/|height| are DIPs scaled by owner/screen DPI.
    // Set false only when the caller already computed physical pixels.
    bool size_in_dips = true;
    FrameKind frame_kind = FrameKind::kSystem;
    // When true, CreateWindow uses |screen_x|/|screen_y| (physical screen px).
    bool has_screen_origin = false;
    int screen_x = 0;
    int screen_y = 0;
    // Combobox / menu-style: close when the popup loses activation.
    bool dismiss_on_deactivate = false;
  };

  Widget();
  virtual ~Widget();

  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;

  bool init(const InitParams& params);
  void set_contents_view(std::unique_ptr<View> contents);
  View* contents_view() const { return contents_.get(); }
  FrameKind frame_kind() const { return frame_kind_; }
  HWND hwnd() const { return hwnd_; }

  // Latest published shell raster (BGRA8, top-down). Valid until the next
  // published generation or destroy. Hosts copy this into
  // gpu::DrawRequest::shell. This widget does not blend.
  ui::gfx::ShellRaster shell_raster() const;

  // Monotonic generation of the last published shell raster (0 = none).
  // GPU hosts can skip upload when this is unchanged.
  std::uint64_t shell_generation() const;

  // Physical pixels per DIP (dpi / 96). Defaults to 1 until init().
  float device_scale_factor() const { return device_scale_factor_; }
  unsigned dpi() const { return dpi_; }

  // Test / programmatic DPI change without a real WM_DPICHANGED.
  void set_device_scale_factor(float scale_factor);

  void show();
  int run_loop();
  // Nested loop until this HWND is destroyed. Does not PostQuitMessage.
  int run_modal();
  void request_close();

  // Invoked once on WM_CLOSE / request_close before DestroyWindow so hosts can
  // drop FlyCube / map HWND userdata while the process heap is still intact.
  using WillClose = std::function<void()>;
  void set_will_close(WillClose fn);

  // UI thread: after a shell generation is published and BitBlt'd (may be a
  // wake paint after async raster). |dirty| is the client rect rastered for
  // that generation. Hosts may copy shell_raster() into
  // MapViewport::commit_shell_overlay / DrawRequest.shell — prefer skipping
  // when |dirty| does not intersect map panes (chrome hover).
  // Cleared automatically in fire_will_close before host teardown.
  using OnShellPublished = std::function<void(const Rect& dirty)>;
  void set_on_shell_published(OnShellPublished fn);

  void layout_contents();
  // Full-client invalidate (resize / theme). Prefer schedule_paint_rect for
  // hover / local control updates so mouse-move does not dirty the whole HWND.
  void schedule_paint();
  void schedule_paint_rect(const Rect& dirty);

  View* focused_view() const { return focused_; }
  View* hovered_view() const { return hovered_; }
  void set_focused_view(View* view);
  void clear_view_refs(View* view);
  bool advance_focus(bool reverse);

  bool send_mouse(const MouseEvent& event);
  bool send_key(const KeyEvent& event);
  bool send_char(const CharEvent& event);

 private:
  friend class WidgetThemeWatch;
  void on_theme_changed();
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam);
  static Widget* from_hwnd(HWND hwnd);

  LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT handle_nc_hit_test(int screen_x, int screen_y);
  void on_paint();
  void on_shell_published_message(std::uint64_t generation);
  void maybe_notify_shell_published(std::uint64_t published_gen);
  void on_size(int width, int height);
  void on_dpi_changed(unsigned new_dpi, const RECT* suggested);
  bool dispatch_mouse(MouseEvent::Type type, WPARAM wparam, LPARAM lparam,
                      int button, int wheel);
  bool dispatch_key(KeyEvent::Type type, WPARAM wparam, LPARAM lparam);
  void track_mouse_leave();
  void update_hover(View* hit);
  void sync_dpi_from_hwnd();
  void union_pending_dirty(const Rect& dirty);
  void take_pending_dirty(int width_px, int height_px, Rect* out);
  bool has_pending_paint() const;
  void shutdown_compositor();

  void fire_will_close();

  HWND hwnd_ = nullptr;
  std::unique_ptr<View> contents_;
  std::unique_ptr<ShellCompositor> compositor_;
  FrameKind frame_kind_ = FrameKind::kSystem;
  View* focused_ = nullptr;
  View* hovered_ = nullptr;
  View* pressed_ = nullptr;
  bool tracking_leave_ = false;
  bool destroying_ = false;
  bool will_close_fired_ = false;
  bool modal_ = false;
  bool dismiss_on_deactivate_ = false;
  // Heap-allocated so Widget stays small on the stack (RTC / teardown safety).
  std::unique_ptr<WillClose> will_close_;
  std::unique_ptr<OnShellPublished> on_shell_published_;
  float device_scale_factor_ = 1.f;
  unsigned dpi_ = kDefaultDpi;
  // Accumulated until the next Commit (prefer schedule_paint_rect).
  Rect pending_dirty_{};
  bool full_paint_pending_ = true;
  // Dirty / generation of the latest Commit awaiting (or just past) publish.
  Rect awaiting_publish_dirty_{};
  std::uint64_t awaiting_publish_gen_ = 0;
  std::uint64_t last_shell_published_notified_ = 0;
  // Coalesce kShellPublishedMessage → one InvalidateRect while paints pending.
  bool shell_wake_invalidate_pending_ = false;
  // U5: coalesce Commits to ~1 per refresh when hover storms WM_PAINT.
  std::int64_t last_commit_qpc_ = 0;
  std::int64_t last_begin_frame_qpc_ = 0;
  // Heap-owned so Widget stays layout-stable across the DLL boundary
  // (no ThemeObserver base; hwnd()/scale stay at fixed offsets for inlines).
  std::unique_ptr<ThemeObserver> theme_watch_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_WIDGET_WIDGET_H_
