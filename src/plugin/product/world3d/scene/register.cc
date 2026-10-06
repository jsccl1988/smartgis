// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/atmosphere/panel.h"
#include "plugin/product/world3d/scene/dem/register.h"
#include "plugin/product/world3d/scene/earth/register.h"
#include "plugin/product/world3d/scene/hexgrid/register.h"
#include "plugin/product/world3d/scene/model/register.h"
#include "plugin/product/world3d/scene/orthogrid/register.h"
#include "plugin/product/world3d/scene/pointcloud/register.h"

namespace plugin {
namespace detail {

bool register_world3d_scene(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  return register_world3d_dem(host) && register_world3d_orthogrid(host) &&
         register_world3d_hexgrid(host) && register_world3d_earth(host) &&
         register_world3d_model(host) && register_world3d_pointcloud(host) &&
         contribute_atmosphere_panel(host);
}

}  // namespace detail
}  // namespace plugin
