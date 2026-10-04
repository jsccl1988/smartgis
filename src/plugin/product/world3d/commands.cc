// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/grid/register.h"
#include "plugin/product/world3d/scene/register.h"

namespace plugin {

bool register_world3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  return detail::register_world3d_grid(host) &&
         detail::register_world3d_scene(host);
}

}  // namespace plugin
