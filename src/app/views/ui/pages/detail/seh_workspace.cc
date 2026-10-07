// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/detail/seh_workspace.h"

#include "app/views/ui/pages/detail/ptr_guard.h"
#include "content/public/view_host.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

tool::Workspace* seh_view_host_workspace(content::ViewHost* host) {
  __try {
    return host->workspace();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

bool seh_view_host_flashing(content::ViewHost* host) {
  if (!host || ptr_addr_poison(reinterpret_cast<uintptr_t>(host))) {
    return false;
  }
  if (!ptr_mem_readable(host, sizeof(void*) * 2)) {
    return false;
  }
  __try {
    tool::Workspace* ws = host->workspace();
    if (!ws || ptr_addr_poison(reinterpret_cast<uintptr_t>(ws)) ||
        !ptr_mem_readable(ws, sizeof(void*) * 2)) {
      return false;
    }
    return ws->flashing();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool seh_bind_workspace(tool::Workspace* ws, WorkspaceBindFns* fns) {
  __try {
    fns->set_draft(ws, fns->draft_ctx);
    fns->set_hit(ws, fns->hit_ctx);
    fns->set_nav(ws, fns->nav_ctx);
    fns->set_project(ws, fns->project_ctx);
    ws->set_shell_owns_append(false);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

void trampoline_set_draft(tool::Workspace* ws, void* ctx) {
  ws->set_draft_observer(*static_cast<tool::DraftCallback*>(ctx));
}

void trampoline_set_hit(tool::Workspace* ws, void* ctx) {
  ws->set_feature_hit(*static_cast<tool::Workspace::FeatureHit*>(ctx));
}

void trampoline_set_nav(tool::Workspace* ws, void* ctx) {
  ws->set_nav_command(*static_cast<tool::Workspace::NavCommand*>(ctx));
}

void trampoline_set_project(tool::Workspace* ws, void* ctx) {
  ws->set_map_project(*static_cast<tool::Workspace::MapProject*>(ctx));
}

}  // namespace detail
}  // namespace app
