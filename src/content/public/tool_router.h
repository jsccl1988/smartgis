// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_TOOL_ROUTER_H
#define CONTENT_PUBLIC_TOOL_ROUTER_H

#include <cstdint>

#include "content/public/map_types.h"

// Chrome input that missed ribbon/tree/dialog. Local in-process path
// uses tool::Workspace; leftover OOP/web still send HostMsg JSON.
namespace content {

class ToolRouter {
 public:
  virtual ~ToolRouter() = default;
  virtual void activate(uint32_t view_id, const char* tool_id) = 0;
  virtual void dispatch(uint32_t view_id, const InputEvent& e) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TOOL_ROUTER_H
