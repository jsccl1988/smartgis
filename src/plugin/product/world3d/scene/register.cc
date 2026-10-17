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
  // Atmosphere dock must still register when a sibling scene register fails.
  // Otherwise the inspector Atmosphere page has no factory to mount.
  const bool dem = register_world3d_dem(host);
  const bool ortho = register_world3d_orthogrid(host);
  const bool hex = register_world3d_hexgrid(host);
  const bool earth = register_world3d_earth(host);
  const bool model = register_world3d_model(host);
  const bool cloud = register_world3d_pointcloud(host);
  const bool atmo = contribute_atmosphere_panel(host);
  return dem && ortho && hex && earth && model && cloud && atmo;
}

}  // namespace detail
}  // namespace plugin
