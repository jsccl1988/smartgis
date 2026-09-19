/*
File:    vw_2dxview.h

Desc:    Smt2DEditXView,Smt 2d edit view 锟教筹拷锟斤拷Smt2DXView

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_2DEDITXVIEW_H
#define _VW_2DEDITXVIEW_H
#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "legacy/ui/xview/view_2d.h"

namespace gis {
class MapEditSession;
}

// Smt2DEditXView 锟斤拷图
namespace ui {
class XVIEW_EXPORT Smt2DEditXView : public Smt2DXView {
  DECLARE_DYNCREATE(Smt2DEditXView)

 protected:
  Smt2DEditXView();  // 锟斤拷态锟斤拷锟斤拷锟斤拷使锟矫碉拷锟杰憋拷锟斤拷锟侥癸拷锟届函锟斤拷
  virtual ~Smt2DEditXView();

 public:
  virtual void OnDraw(CDC* pDC);  // 锟斤拷写锟皆伙拷锟狡革拷锟斤拷图
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

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  void SetOperMap(SmtMap* pSmtMap);

 protected:
  bool InitCreate(void);
  bool EndDestory(void);
  bool CreateContexMenu();
  bool CreateMainMenu();

  bool CreateTools(void);

 protected:
  SmtBaseTool* m_pAppendFeaTool;
  gis::MapEditSession* m_pMapEdits;
};
}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_VW_2DEDITXVIEW_H