// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_TOOL_SESSION_H_
#define CONTENT_PUBLIC_TOOL_SESSION_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "content/content_export.h"
#include "content/public/types.h"

namespace gis {
class EditSession;
}

namespace tool {
class Workspace;
}

namespace content {

class EventBus;

// Per-view tool/edit composition (Workspace + EventBus + EditSession).
// Not the document root (see GisContents) and not process hooks (GisContentsClient).
// Public surface has no HWND, Map*, or LPRENDERDEVICE.
class CONTENT_EXPORT ToolSession {
 public:
  ToolSession();
  explicit ToolSession(gis::EditSession* edits);
  ~ToolSession();

  ToolSession(const ToolSession&) = delete;
  ToolSession& operator=(const ToolSession&) = delete;

  EventBus* events();
  gis::EditSession* edits();
  tool::Workspace* workspace();

  bool execute(std::string_view command_id, uint32_t view_id = 0);
  bool activate(std::string_view interaction_id);
  bool dispatch_input(const InputEvent& e);
  // True if gt_msg maps via command_id_from_gt_msg (then execute).
  // False if unmapped: shell must not broadcast leftover plugins.
  bool execute_legacy(long gt_msg);
  void release_exclusive();
  bool flashing() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TOOL_SESSION_H_
