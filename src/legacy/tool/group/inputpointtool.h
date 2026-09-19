/*
File:    gt_inputpointtool.h

Desc:    SmtInputPointTool,���ӵ�Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INPUT_POINT_H
#define _GT_INPUT_POINT_H

#include "legacy/tool/group/basetool.h"
#include "tool/gestures.h"

namespace tool {
// Digitize shell kept for GTT_InputPoint factory ABI (plugins / CreateGroupTool).
// Pointer state machine removed: geometry only via apply_draft (chrome observer
// or bound Workspace → active IA). Bound Append uses draw.* and never
// BeginDelegate into this tool. Callers that need exclusive IA must SetActive.
class SmtInputPointTool : public SmtBaseTool {
 public:
  SmtInputPointTool();
  virtual ~SmtInputPointTool();

  int Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap *pOperSmtMap, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void *pToFollow = NULL);
  int AuxDraw();

 public:
  int notify(long nMsg, SmtListenerMsg &param);
  void apply_draft(const tool::Draft &draft) override;

 protected:
  void OnSetPointType(void);
  void EndAppendPoint();

 protected:
  OGRGeometry *m_pGeom;
  ushort m_appendType;
  float m_fAngle;
};
}  // namespace tool

#endif  //_GT_INPUT_POINT_H
