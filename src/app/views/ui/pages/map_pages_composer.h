// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PAGES_MAP_PAGES_COMPOSER_H_
#define APP_VIEWS_UI_PAGES_MAP_PAGES_COMPOSER_H_

#include <functional>
#include <string>

namespace content {
class MapHwndGestures;
class ViewHost;
}  // namespace content

namespace ui {
namespace gfx {
struct Rect;
}  // namespace gfx
namespace views {
class DrawHost;
using Rect = ui::gfx::Rect;
}  // namespace views
}  // namespace ui

namespace app {

class BrowserView;

// Map/Data/3D tab horizon: viewports, gestures, overlays, tool seams.
class MapPagesComposer {
 public:
  explicit MapPagesComposer(BrowserView* host);
  ~MapPagesComposer() = default;

  MapPagesComposer(const MapPagesComposer&) = delete;
  MapPagesComposer& operator=(const MapPagesComposer&) = delete;

  void attach_viewports();
  void wire_map_scene();
  void commit_widget_shell_to_maps();
  void commit_widget_shell_to_maps(const ui::views::Rect& dirty);
  void sync_flash_timer();
  void wire_tool_seams();
  void for_each_draw_host(const std::function<void(ui::views::DrawHost*)>& fn) const;
  void invalidate_map_overlays();
  void attach_hwnd_gestures();
  void configure_gestures(content::MapHwndGestures* gestures);
  void active_view_size(int* w, int* h) const;
  void switch_map_tab(int i);
  ui::views::DrawHost* active_map() const;
  content::ViewHost* active_view_host() const;

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PAGES_MAP_PAGES_COMPOSER_H_
