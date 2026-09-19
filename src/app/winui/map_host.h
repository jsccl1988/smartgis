// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_MAP_HOST_H_
#define APP_WINUI_MAP_HOST_H_

#include <windows.h>

#include <string>
#include <string_view>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "app/views/blit_frame_cache.h"
#include "app/views/map_scene.h"
#include "app/views/scene3d_controller.h"
#include "app/winui/detail/map_session.h"
#include "content/public/map_contents_observer.h"

namespace app {
namespace winui {

// Presents the map inside the WinUI tree. GPU publishes software-DIB shared
// pixels; this host owns an **owned WS_POPUP** HWND (owner = top-level
// window) aligned to the SwapChainPanel slot in **screen** coordinates.
// A WS_CHILD sibling is covered by DesktopChildSiteBridge composition
// (PrintWindow on child shows the map; the main window does not). Popup
// sits above XAML. Blits Latest() + MapScene / Scene3dController like Views.
// Map/Data/3D views stay open across tab switches — never CloseView just to
// change chrome tabs.
class MapHost : public content::MapContentsObserver {
 public:
  MapHost();
  ~MapHost() override;

  MapHost(const MapHost&) = delete;
  MapHost& operator=(const MapHost&) = delete;

  winrt::Microsoft::UI::Xaml::Controls::Grid root_element() const;

  void attach_session(content::MapContents* session, HWND window_hwnd);
  // Open Map Edit / Data / Scene3d (keeps prior views alive).
  void show_kind(content::ViewKind kind);
  void sync_layout();
  const wchar_t* present_path() const;
  const wchar_t* process_path() const;
  content::ViewKind view_kind() const { return kind_; }

  // Self-test / diagnostics: child HWND after attach, and whether sync_layout
  // produced a non-trivial client rect (top-level coords aligned to the panel).
  HWND map_child_hwnd() const { return child_hwnd_; }
  bool has_synced_map_layout() const;
  // True when HostView::Latest has a shared DIB (generation + handle).
  bool has_presented_frame() const;
  // True when map vectors are loaded or GPU pixels are not the placeholder.
  bool has_live_map_pixels() const;
  uint32_t view_id() const { return view_id_; }
  content::MapContents* session() const { return session_; }
  ::app::MapScene* map_scene() { return &map_scene_; }
  const ::app::MapScene* map_scene() const { return &map_scene_; }

  // Open a vector path via OGR (Views parity). Also forwards CatalogCall.
  bool open_map_path(const std::string& path);

  // Workspace tool id for HWND / XAML pointer gestures (pan / zoom / select).
  void set_active_tool(std::string_view tool_id);
  void apply_pointer(const content::InputEvent& ev);

  // Hide the HWND overlay when chrome covers the map slot (unused in IDE layout).
  void set_map_surface_visible(bool visible);

  void OnFrameReady(uint32_t view_id, uint32_t generation) override;

  static LRESULT CALLBACK child_wnd_proc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam);

 private:
  struct ViewSlot {
    uint32_t view_id = 0;
    content::MapWidgetHostView* view = nullptr;
  };

  void attach_child_hwnd();
  void destroy_child_hwnd();
  void paint_child() const;
  void paint_to_dc(HDC hdc, const RECT& rc) const;
  bool present_latest_frame(HDC hdc, const RECT& client_rc) const;
  void start_present_timer();
  void stop_present_timer();
  HWND resolve_island_hwnd() const;
  float panel_scale() const;
  void close_all_views();
  void update_status_overlay();
  void wire_panel_pointers();
  static int slot_index(content::ViewKind kind);

  static constexpr UINT_PTR kPresentTimerId = 1;
  static constexpr UINT_PTR kBlitTimerId = 2;

  void schedule_full_redraw();
  void commit_blit_preview();

  mutable BlitFrameCache blit_;

  winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};

  content::MapContents* session_ = nullptr;
  content::MapWidgetHostView* view_ = nullptr;
  ViewSlot slots_[3] = {};
  ::app::MapScene map_scene_;
  ::app::Scene3dController scene3d_;
  HWND window_hwnd_ = nullptr;
  HWND island_hwnd_ = nullptr;
  HWND child_hwnd_ = nullptr;
  uint32_t view_id_ = 0;
  uint32_t painted_generation_ = 0;
  int last_layout_x_ = -1;
  int last_layout_y_ = -1;
  int last_layout_w_ = -1;
  int last_layout_h_ = -1;
  content::ViewKind kind_ = content::ViewKind::kMapEdit;
  std::string active_tool_{"view.pan"};
  bool dragging_ = false;
  int last_pointer_x_ = 0;
  int last_pointer_y_ = 0;
};

}  // namespace winui
}  // namespace app

#endif  // APP_WINUI_MAP_HOST_H_
