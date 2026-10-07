// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PAGES_DETAIL_SEH_WORKSPACE_H_
#define APP_VIEWS_UI_PAGES_DETAIL_SEH_WORKSPACE_H_

namespace content {
class ViewHost;
}  // namespace content

namespace tool {
class Workspace;
}  // namespace tool

namespace app {
namespace detail {

// SEH wrappers live in a TU with no C++ object unwinding in the __try body
// (MSVC C2712). Used when BrowserSession / ViewHost ABI drifts across partial
// multi-agent out/Debug rebuilds.

tool::Workspace* seh_view_host_workspace(content::ViewHost* host);
bool seh_view_host_flashing(content::ViewHost* host);

struct WorkspaceBindFns {
  void (*set_draft)(tool::Workspace*, void*);
  void (*set_hit)(tool::Workspace*, void*);
  void (*set_nav)(tool::Workspace*, void*);
  void (*set_project)(tool::Workspace*, void*);
  void* draft_ctx;
  void* hit_ctx;
  void* nav_ctx;
  void* project_ctx;
};

bool seh_bind_workspace(tool::Workspace* ws, WorkspaceBindFns* fns);

void trampoline_set_draft(tool::Workspace* ws, void* ctx);
void trampoline_set_hit(tool::Workspace* ws, void* ctx);
void trampoline_set_nav(tool::Workspace* ws, void* ctx);
void trampoline_set_project(tool::Workspace* ws, void* ctx);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UI_PAGES_DETAIL_SEH_WORKSPACE_H_
