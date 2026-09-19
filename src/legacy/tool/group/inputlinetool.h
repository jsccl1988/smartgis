/*
File:    gt_inputlinetool.h

Desc:    SmtInputLineTool,������Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INIPUT_LINE_H
#define _GT_INIPUT_LINE_H

#include "legacy/tool/group/basetool.h"
#include "tool/gestures.h"

namespace tool {
// Digitize shell kept for GTT_InputLine factory ABI (e.g. orthogrid plugin).
// Pointer state machine removed: geometry only via apply_draft. Bound Append
// uses draw.linestring and never BeginDelegate here. Callers SetActive.
class SmtInputLineTool : public SmtBaseTool {
 public:
  SmtInputLineTool();
  virtual ~SmtInputLineTool();

  int Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap *pOperSmtMap, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void *pToFollow = NULL);
  int AuxDraw();

 public:
  int notify(long nMsg, SmtListenerMsg &param);
  void apply_draft(const tool::Draft &draft) override;

 protected:
  void OnSetLineType(void);
  void EndAppendLine();

 protected:
  OGRGeometry *m_pGeom;
  ushort m_appendType;
};
}  // namespace tool

#endif  //_GT_INIPUT_LINE_H
