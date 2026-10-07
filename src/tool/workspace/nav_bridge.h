// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_WORKSPACE_NAV_BRIDGE_H_
#define TOOL_WORKSPACE_NAV_BRIDGE_H_

#include <functional>
#include <string_view>

#include "content/public/types.h"
#include "tool/command/command.h"

namespace content {
class EventBus;
}

namespace tool {
namespace detail {

// Bridges view.full / view.refresh to the shell camera, then publishes extent.
class NavBridge {
 public:
  using NavCommand =
      std::function<content::Extent2(std::string_view command_id)>;

  void set_nav_command(NavCommand fn);
  bool publish_nav(std::string_view command_id, const CommandArgs& args,
                   content::EventBus* events);

 private:
  NavCommand nav_command_;
};

}  // namespace detail
}  // namespace tool

#endif  // TOOL_WORKSPACE_NAV_BRIDGE_H_
