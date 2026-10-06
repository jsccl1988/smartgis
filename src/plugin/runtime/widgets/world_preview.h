// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_WORLD_PREVIEW_H_
#define PLUGIN_WIDGETS_WORLD_PREVIEW_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/view/view.h"

namespace plugin {

// Shared Scene3d / globe preview for world3d-style product horizon. Owns a
// DrawHost (Role::kScene3d) plus a StatusBar for attach / document text.
class PLUGIN_HOST_EXPORT WorldPreviewView : public ui::views::View {
 public:
  WorldPreviewView();
  bool open_document(std::string_view path);
  // False when the draw host has no presented pixels.
  bool export_bmp(const std::string& path) const;
  ui::views::DrawHost* draw_host();
  ui::views::StatusBar* status_bar();
  const std::string& document_path() const;

 private:
  ui::views::DrawHost* draw_host_ = nullptr;
  ui::views::StatusBar* status_ = nullptr;
  std::string path_;
};

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_WORLD_PREVIEW_H_
