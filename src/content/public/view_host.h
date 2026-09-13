// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_VIEW_HOST_H_
#define CONTENT_PUBLIC_VIEW_HOST_H_

#include <cstdint>
#include <memory>
#include <string_view>

#include "content/public/map_types.h"

namespace sdb {
class EditSession;
}

namespace tool {
class Workspace;
}

namespace content {

class EventBus;

// Per-map-view chrome composition: Workspace + EventBus + EditSession.
// Public surface has no HWND, SmtMap*, or LPRENDERDEVICE.
class ViewHost {
 public:
  ViewHost();
  explicit ViewHost(sdb::EditSession* edits);
  ~ViewHost();

  ViewHost(const ViewHost&) = delete;
  ViewHost& operator=(const ViewHost&) = delete;

  EventBus* events();
  sdb::EditSession* edits();
  tool::Workspace* workspace();

  bool execute(std::string_view command_id, uint32_t view_id = 0);
  bool activate(std::string_view interaction_id);
  bool dispatch_input(const InputEvent& e);
  // True if gt_msg maps via command_id_from_gt_msg (then execute).
  // False if unmapped: chrome must not broadcast leftover plugins.
  bool execute_legacy(long gt_msg);
  void release_exclusive();
  bool flashing() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_VIEW_HOST_H_
