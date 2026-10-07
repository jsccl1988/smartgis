// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_MAP2D_BRIDGES_H_
#define APP_VIEWS_BROWSER_PLUGIN_MAP2D_BRIDGES_H_

#include "app/views/browser/plugin/host_context.h"

namespace app {
namespace detail {

// Installs Map2dSink bridges on |ctx.host| from horizon-filled callbacks.
void install_map2d_host_bridges(const Map2dHostContext& ctx);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_MAP2D_BRIDGES_H_
