// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_CEF_MAP_SLOT_H_
#define APP_CEF_CEF_MAP_SLOT_H_

#include <cstdint>
#include <functional>

#include "app/cef/layout_host.h"
#include "content/public/map_types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
class MapContents;
class MapWidgetHostView;
class ViewHost;
}  // namespace content

namespace app {
class MapScene;

namespace cef {

// Sibling map HWND (not under the CEF control tree). Presents MapContents
// software-DIB frames, overlays MapScene vectors, and forwards pointer input.
class CefMapSlot {
 public:
  using ViewMenuRequested = std::function<void(POINT screen)>;

  CefMapSlot();
  ~CefMapSlot();

  CefMapSlot(const CefMapSlot&) = delete;
  CefMapSlot& operator=(const CefMapSlot&) = delete;

  bool create(HWND parent,
              content::MapContents* session,
              content::ViewKind kind);
  void destroy();
  void sync_layout(const RectPx& rect_px, float dpi);
  void set_visible(bool visible);
  bool wait_ready(uint32_t timeout_ms);
  // True when Latest() has a non-zero generation and pixel dimensions.
  bool has_presented_frame() const;
  uint32_t presented_generation() const;

  HWND native_hwnd() const { return child_hwnd_; }
  uint32_t view_id() const { return view_id_; }
  content::ViewHost* view_host();
  content::ViewKind kind() const { return kind_; }
  // BrowserMain sets whether MapContents::StartRenderProcess succeeded.
  void set_render_ok(bool ok) { render_ok_ = ok; }
  void set_document(MapScene* document) { document_ = document; }
  void set_view_menu_requested(ViewMenuRequested fn) {
    view_menu_requested_ = std::move(fn);
  }
  void invalidate() {
    if (child_hwnd_) {
      InvalidateRect(child_hwnd_, nullptr, FALSE);
    }
  }

  static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

 private:
  void paint_child();
  void paint_to_dc(HDC hdc, const RECT& rc);
  bool present_latest_frame(HDC hdc, const RECT& client_rc);
  void dispatch_mouse(content::InputEvent::Kind kind, LPARAM lparam, int wheel);
  void start_present_timer();
  void stop_present_timer();
  void show_view_menu(POINT screen_pt) const;

  static constexpr UINT_PTR kPresentTimerId = 1;

  HWND parent_ = nullptr;
  HWND child_hwnd_ = nullptr;
  content::MapContents* session_ = nullptr;
  content::MapWidgetHostView* view_ = nullptr;
  content::ViewHost* view_host_ = nullptr;
  MapScene* document_ = nullptr;
  ViewMenuRequested view_menu_requested_;
  uint32_t view_id_ = 0;
  content::ViewKind kind_ = content::ViewKind::kMapEdit;
  bool visible_ = false;
  bool render_ok_ = true;
  int last_layout_x_ = 0;
  int last_layout_y_ = 0;
  int last_layout_w_ = 0;
  int last_layout_h_ = 0;
  float last_dpi_ = 0.f;
  uint32_t painted_generation_ = 0;
};

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_CEF_MAP_SLOT_H_
