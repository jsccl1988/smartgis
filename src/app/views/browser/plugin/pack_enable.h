// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_PACK_ENABLE_H_
#define APP_VIEWS_BROWSER_PLUGIN_PACK_ENABLE_H_

namespace plugin {
class Registry;
}  // namespace plugin

namespace app {
namespace detail {

// Wire Registry::set_enabled as the command-pack ensure callback. Pass null
// registry to clear.
void install_command_pack_enable(plugin::Registry* registry);
void clear_command_pack_enable();

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_PACK_ENABLE_H_
