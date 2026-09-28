// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/workspace/nav_bridge.h"

#include "content/public/event_bus.h"

namespace tool {
namespace detail {

void NavBridge::set_nav_command(NavCommand fn) {
  nav_command_ = std::move(fn);
}

bool NavBridge::publish_nav(std::string_view command_id,
                            const CommandArgs& args,
                            content::EventBus* events) {
  content::Extent2 extent{};
  if (nav_command_) {
    extent = nav_command_(command_id);
  }
  if (events) {
    content::ExtentChanged ev;
    ev.view_id = args.view_id;
    ev.extent = extent;
    events->publish(ev);
  }
  return true;
}

}  // namespace detail
}  // namespace tool
