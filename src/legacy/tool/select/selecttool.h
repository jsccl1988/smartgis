// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GT_SELECTTOOL_H
#define _GT_SELECTTOOL_H

#include "legacy/gis/datasource/datasource_mgr.h"
#include "legacy/tool/base/basetool.h"
#include "tool/draft/draft.h"

namespace tool {
class Workspace;
}

namespace tool {
class SmtSelectTool : public SmtBaseTool {
 public:
  SmtSelectTool();
  virtual ~SmtSelectTool();

  int Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void* pToFollow = NULL);
  int AuxDraw();
  int Timer();

 public:
  int KeyDown(uint nChar, uint nRepCnt, uint nFlags);

 public:
  int notify(long nMsg, SmtListenerMsg& param);
  void apply_draft(const tool::Draft& draft) override;

  // Bound: pointer owned by Workspace — leftover handlers are no-ops.
  int LButtonDown(uint nFlags, lPoint point) override;
  int LButtonUp(uint nFlags, lPoint point) override;
  int MouseMove(uint nFlags, lPoint point) override;

  // When bound, mapped GT_MSG_* activate Workspace select.* / selection.clear.
  void bind_workspace(tool::Workspace* workspace) { m_workspace = workspace; }

 protected:
  void OnRetDelegate(int nRetType);
  void OnSetSelMode(void);

 protected:
  eSelectMode m_selMode;
  int m_nLayerFeaType;

  gis::ScratchLayer m_resultLayer;
  SmtGQueryDesc m_gQDes;
  SmtPQueryDesc m_pQDes;
  double m_dpMargin;
  tool::Workspace* m_workspace = nullptr;
};
}  // namespace tool

#endif  //_GT_SELECTTOOL_H
