/*
File:    vw_2dxview.h

Desc:    Smt2DXView,Smt 2d view 锟教筹拷锟斤拷SmtXView

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_2DXVIEW_H
#define _VW_2DXVIEW_H
#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "legacy_render/bridge/renderdevice.h"
#include "legacy_render/bridge/renderer.h"
#include "sdb/map/map.h"
#include "legacy_tool/group/grouptoolfactory.h"
#include "ui/xview/xview.h"

using namespace render;
using namespace tool;
using namespace sdb;

// Smt2DXView 锟斤拷图
namespace ui {
class XVIEW_EXPORT Smt2DXView : public SmtXView {
  DECLARE_DYNCREATE(Smt2DXView)

 public:
  Smt2DXView();
  virtual ~Smt2DXView();

 public:
  LPRENDERDEVICE GetRenderDevice(void);

 public:
  virtual void OnDraw(CDC* pDC);  // 锟斤拷写锟皆伙拷锟狡革拷锟斤拷图
  virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
  afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

  afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

 public:
  virtual void SetOperMap(SmtMap* pSmtMap);
  SmtMap* GetOperMap(void);

 protected:
  bool InitCreate(void);
  bool EndDestory(void);
  bool CreateContexMenu();
  bool CreateMainMenu(void);

  bool CreateRender(void);
  bool CreateTools(void);
  void apply_workspace_draft(const tool::Draft& draft) override;

 protected:
  UINT m_uiNotifyTimer;
  UINT m_uiRefreshTimer;

  LPRENDERER m_pRenderer;
  LPRENDERDEVICE m_pRenderDevice;

  SmtBaseTool* m_pViewCtrlTool;
  SmtBaseTool* m_pSelectTool;
  SmtBaseTool* m_pFlashTool;

  SmtMap* m_pSmtOperMap;
};
}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_VW_2DXVIEW_H