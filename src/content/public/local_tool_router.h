// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_LOCAL_TOOL_ROUTER_H_
#define CONTENT_PUBLIC_LOCAL_TOOL_ROUTER_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>

#include "content/public/map_types.h"
#include "content/public/tool_router.h"
#include "content/public/view_host.h"

namespace sdb {
class EditSession;
}

namespace content {

// In-process ToolRouter: per-view Workspace first, leftover IPC optional.
class LocalToolRouter final : public ToolRouter {
 public:
  using ActivateIpc = std::function<void(uint32_t view_id, const char* tool_id)>;
  using DispatchIpc =
      std::function<void(uint32_t view_id, const InputEvent& e)>;

  LocalToolRouter();
  ~LocalToolRouter() override;

  ViewHost* host(uint32_t view_id);
  void close(uint32_t view_id);
  void bind_edits(uint32_t view_id, sdb::EditSession* edits);

  void set_activate_ipc(ActivateIpc fn);
  void set_dispatch_ipc(DispatchIpc fn);

  void activate(uint32_t view_id, const char* tool_id) override;
  void dispatch(uint32_t view_id, const InputEvent& e) override;

 private:
  std::map<uint32_t, std::unique_ptr<ViewHost>> hosts_;
  ActivateIpc activate_ipc_;
  DispatchIpc dispatch_ipc_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_LOCAL_TOOL_ROUTER_H_
