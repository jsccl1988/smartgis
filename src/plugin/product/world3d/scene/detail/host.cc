// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/detail/host.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "ui/views/dialogs/message_box.h"

#include <string>

namespace plugin {
namespace detail {

Scene3dSink* world3d_scene_sink(content::PluginHost* host) {
  return plugin::scene3d_sink(host);
}

bool world3d_earth_ready(content::PluginHost* host) {
  Scene3dSink* sink = world3d_scene_sink(host);
  return sink && sink->earth_bridges_installed();
}

bool fail_no_scene(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  ui::views::show_message_box(
      ui::views::MessageBoxKind::kError,
      "No scene render device is attached to the host.");
  return false;
}

bool fail_no_scene_processing(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  return false;
}

}  // namespace detail
}  // namespace plugin
