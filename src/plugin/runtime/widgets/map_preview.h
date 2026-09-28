// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_MAP_PREVIEW_H_
#define PLUGIN_WIDGETS_MAP_PREVIEW_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/kernel/view/view.h"

namespace plugin {

// Shared map preview used by print. Owns a MapViewport
// plus a StatusBar for attach / document text.
class PLUGIN_HOST_EXPORT MapPreviewView : public ui::views::View {
 public:
  MapPreviewView();
  bool open_document(std::string_view path);
  // False when the viewport has no presented pixels.
  bool export_bmp(const std::string& path) const;
  ui::views::MapViewport* viewport();
  ui::views::StatusBar* status_bar();
  const std::string& document_path() const;

 private:
  ui::views::MapViewport* viewport_ = nullptr;
  ui::views::StatusBar* status_ = nullptr;
  std::string path_;
};

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_MAP_PREVIEW_H_
