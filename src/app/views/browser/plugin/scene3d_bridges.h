// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_SCENE3D_BRIDGES_H_
#define APP_VIEWS_BROWSER_PLUGIN_SCENE3D_BRIDGES_H_

#include "app/views/browser/plugin/host_context.h"

namespace app {
namespace detail {

// Installs Scene3dSink bridges on |ctx.host| from horizon-filled callbacks.
void install_scene3d_host_bridges(const Scene3dHostContext& ctx);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_SCENE3D_BRIDGES_H_
