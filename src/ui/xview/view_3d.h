/*
File:    vw_xview.h

Desc:    Smt3DXView,Smt 3d view �̳���SmtXView

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_3DXVIEW_H
#define _VW_3DXVIEW_H
#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "legacy_render/render3d/3drenderdevice.h"
#include "legacy_render/render3d/3drenderer.h"
#include "legacy_render/scene3d/bl3d_scene.h"
#include "legacy_tool/group/grouptoolfactory.h"
#include "ui/xview/xview.h"

using namespace tool;
using namespace render;

// Smt3DXView ��ͼ

namespace ui {
class XVIEW_EXPORT Smt3DXView : public SmtXView {
  DECLARE_DYNCREATE(Smt3DXView)

 public:
  Smt3DXView();
  virtual ~Smt3DXView();

 public:
  LP3DRENDERDEVICE GetRenderDevice(void);
  SmtScene* GetScene(void);

 public:
  virtual void OnDraw(CDC* pDC);  // ��д�Ի��Ƹ���ͼ
  virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
  virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
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

 protected:
  bool InitCreate(void);
  bool EndDestory(void);
  bool CreateContexMenu();
  bool CreateMainMenu(void);

  bool CreateRender(void);
  bool CreateTools(void);
  void apply_workspace_draft(const tool::Draft& draft) override;

  virtual bool Setup(void);

 protected:
  UINT m_uiRefreshTimer;

  LPSMT3DRENDERER m_p3DRenderer;
  LP3DRENDERDEVICE m_p3DRenderDevice;
  SmtScene* m_pScene;

  SmtBase3DTool* m_p3DViewCtrlTool;

  Vector3 m_vCursor3DPos;
};
}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "xviewD.lib")
#else
#pragma comment(lib, "xview.lib")
#endif
#endif
#endif  //_VW_3DXVIEW_H
