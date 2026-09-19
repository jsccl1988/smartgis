/*
File:    gt_appendfeaturetool.h

Desc:    SmtAppendFeatureTool,����Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_APPEND_FEATURE_H
#define _GT_APPEND_FEATURE_H

#include <memory>

#include "legacy/tool/group/basetool.h"
#include "gis/edit/map_edit_session.h"
#include "tool/gestures.h"

namespace tool {
class Workspace;
}

namespace tool {
class SmtAppendFeatureTool : public SmtBaseTool {
 public:
  SmtAppendFeatureTool();
  virtual ~SmtAppendFeatureTool();
  int Create();
  int Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap *pOperSmtMap, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void *pToFollow = NULL);
  int AuxDraw();
  int Timer();

 public:
  inline virtual void SetOperMap(SmtMap *pOperSmtMap);

 public:
  int notify(long nMsg, SmtListenerMsg &param);

 public:
  int KeyDown(uint nChar, uint nRepCnt, uint nFlags);
  void apply_draft(const tool::Draft &draft) override;

  // Bound: pointer owned by Workspace — leftover handlers are no-ops.
  int LButtonDown(uint nFlags, lPoint point) override;
  int LButtonUp(uint nFlags, lPoint point) override;
  int MouseMove(uint nFlags, lPoint point) override;

  // When bound, mapped GT_MSG_APPEND_* activate Workspace draw.*; leftover
  // keeps digitize kind for apply_draft append.
  void bind_workspace(tool::Workspace *workspace) { m_workspace = workspace; }

 protected:
  void OnInputPointFeature(ushort unType);
  void OnInputLineFeature(ushort unType);
  void OnInputRegionFeature(ushort unType);

 protected:
  void AppendPointFeature(ushort unType);
  void AppendChildImageFeature();
  void AppendTextFeature(const char *szAnno, float fangle);
  void AppendDotFeature();

  void AppendLineFeature(void);
  void AppendRegionFeature(void);

 protected:
  OGRGeometry *m_pGeom;
  string m_strAnno;
  ushort m_pointType;
  ushort m_lineType;
  ushort m_regionType;
  int m_digitizeKind;
  std::unique_ptr<gis::MapEditSession> m_edits;
  tool::Workspace *m_workspace = nullptr;
};
}  // namespace tool

#endif  //_GT_APPEND_FEATURE_H
