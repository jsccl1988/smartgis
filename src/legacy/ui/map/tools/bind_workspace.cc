// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/tools/bind_workspace.h"

#include "legacy/core/listener/listener_manager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/draft/appendfeaturetool.h"
#include "legacy/tool/nav/viewctrltool.h"
#include "legacy/tool/select/flashtool.h"
#include "legacy/tool/select/selecttool.h"
#include "tool/workspace/workspace.h"

namespace ui {
namespace detail {

void bind_2d_tools_workspace(tool::SmtBaseTool* view_ctrl,
                             tool::SmtBaseTool* select,
                             tool::SmtBaseTool* flash,
                             tool::SmtBaseTool* append,
                             tool::Workspace* ws,
                             HWND hwnd) {
  if (auto* ctrl = dynamic_cast<SmtViewCtrlTool*>(view_ctrl)) {
    ctrl->bind_workspace(ws);
  }
  if (auto* sel = dynamic_cast<SmtSelectTool*>(select)) {
    sel->bind_workspace(ws);
  }
  if (auto* fl = dynamic_cast<SmtFlashTool*>(flash)) {
    fl->bind_workspace(ws);
  }
  if (auto* app = dynamic_cast<SmtAppendFeatureTool*>(append)) {
    app->bind_workspace(ws);
  }
  if (ws) {
    ws->activate("view.pan");
  }
  if (auto* ctrl = dynamic_cast<SmtViewCtrlTool*>(view_ctrl)) {
    SmtListenerMsg mode_param{};
    mode_param.hSrcWnd = hwnd;
    ctrl->notify(GT_MSG_VIEW_ZOOMMOVE, mode_param);
  }
}

}  // namespace detail
}  // namespace ui
