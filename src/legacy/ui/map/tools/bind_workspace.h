// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_TOOLS_BIND_WORKSPACE_H_
#define LEGACY_UI_MAP_TOOLS_BIND_WORKSPACE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace tool {
class SmtBaseTool;
class Workspace;
}  // namespace tool

namespace ui {
namespace detail {

// Bind leftover 2D group tools to |ws|, activate view.pan, align leftover
// ViewCtrl mode to ZOOMMOVE so pan cursor matches Workspace.
void bind_2d_tools_workspace(tool::SmtBaseTool* view_ctrl,
                             tool::SmtBaseTool* select,
                             tool::SmtBaseTool* flash,
                             tool::SmtBaseTool* append,
                             tool::Workspace* ws,
                             HWND hwnd);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_MAP_TOOLS_BIND_WORKSPACE_H_
