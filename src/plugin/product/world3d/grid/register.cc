// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/register.h"

#include "plugin/product/world3d/grid/dem/register.h"
#include "plugin/product/world3d/grid/hexgrid/register.h"
#include "plugin/product/world3d/grid/orthogrid/register.h"

namespace plugin {
namespace detail {

bool register_world3d_grid(content::PluginHost* host) {
  return register_world3d_dem(host) && register_world3d_orthogrid(host) &&
         register_world3d_hexgrid(host);
}

}  // namespace detail
}  // namespace plugin
