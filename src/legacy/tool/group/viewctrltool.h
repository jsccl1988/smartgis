/*
File:    gt_viewtool.h

Desc:    SmtViewCtrlTool,�������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_2DVIEW_CONTROL_TOOL_H
#define _GT_2DVIEW_CONTROL_TOOL_H

#include "legacy/tool/group/basetool.h"

namespace tool {
class Workspace;
}

namespace tool {
enum CursorType {
  CursorLoupePlus,
  CursorLoupeMinus,
  CursorMove,
  CursorIdentify
};

class SmtViewCtrlTool : public SmtBaseTool {
 public:
  SmtViewCtrlTool();
  virtual ~SmtViewCtrlTool();

  int Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void* pToFollow = NULL);
  int AuxDraw();
  int Timer();

 public:
  int notify(long nMsg, SmtListenerMsg& param);

  int SetCursor(void);
  // Bound: pointer/wheel owned by Workspace — leftover handlers are no-ops.
  int LButtonDown(uint nFlags, lPoint point) override;
  int LButtonUp(uint nFlags, lPoint point) override;
  int MouseMove(uint nFlags, lPoint point) override;
  int MouseWeel(uint nFlags, short zDelta, lPoint point) override;
  void apply_draft(const tool::Draft& draft) override;

  // When bound, mapped GT_MSG_* activate Workspace interactions; leftover
  // keeps m_viewMode + camera side effects for apply_draft.
  void bind_workspace(tool::Workspace* workspace) { m_workspace = workspace; }

 protected:
  void ZoomMove(short mouse_status, lPoint point);
  void ZoomIn(short mouse_status, lPoint point);
  void ZoomOut(short mouse_status, lPoint point);
  void ZoomRestore();
  void ZoomRefresh();
  void OnSetViewMode(void);
  void ApplyWheel(int z_delta, lPoint point);

 protected:
  eViewMode m_viewMode;
  ushort m_usFlashed;
  tool::Workspace* m_workspace = nullptr;

 protected:
  lPoint m_pntOrigin;
  lPoint m_pntCur;
  lPoint m_pntPrev;
  BOOL m_bCaptured;
  HCURSOR m_hCursors[4];
};
}  // namespace tool

#endif  //_GT_2DVIEW_CONTROL_TOOL_H
