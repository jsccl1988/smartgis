// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_PLUGIN_PREVIEW_HOST_H_
#define APP_VIEWS_SHELL_RUNTIME_PLUGIN_PREVIEW_HOST_H_

#include <memory>
#include <string>
#include <string_view>

namespace ui {
namespace views {
class Widget;
}
}  // namespace ui

namespace plugin {
class MapPreviewView;
class WorldPreviewView;
}  // namespace plugin

namespace app {

class Browser;

// Modeless shared MapPreviewView / WorldPreviewView for PluginHost
// present_dataset(surface=1). Owned by Browser; chrome wires DrawHost to the
// same ViewHost / MapContents as the main panes.
class PluginPreviewHost {
 public:
  PluginPreviewHost();
  ~PluginPreviewHost();

  PluginPreviewHost(const PluginPreviewHost&) = delete;
  PluginPreviewHost& operator=(const PluginPreviewHost&) = delete;

  // face 0 → MapPreviewView, face 1 → WorldPreviewView.
  bool present(Browser* browser, int face, std::string_view path);
  void close();
  bool export_bmp(const std::string& path) const;
  int face() const { return face_; }
  bool is_open() const;

 private:
  bool ensure_widget(Browser* browser, int face);
  void wire_draw_host(Browser* browser);

  std::unique_ptr<ui::views::Widget> widget_;
  plugin::MapPreviewView* map_preview_ = nullptr;
  plugin::WorldPreviewView* world_preview_ = nullptr;
  int face_ = -1;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_PLUGIN_PREVIEW_HOST_H_
