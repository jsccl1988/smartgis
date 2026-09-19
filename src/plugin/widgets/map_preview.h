// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_MAP_PREVIEW_H_
#define PLUGIN_WIDGETS_MAP_PREVIEW_H_

#include <string>
#include <string_view>

#include "ui/views/map/map_viewport.h"
#include "ui/views/gis/status_bar.h"
#include "ui/views/kernel/view.h"

namespace plugin {

// Shared map preview used by print. Owns a MapViewport
// plus a StatusBar for attach / document text.
class MapPreviewView : public ui::views::View {
 public:
  MapPreviewView();
  bool open_document(std::string_view path);
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
