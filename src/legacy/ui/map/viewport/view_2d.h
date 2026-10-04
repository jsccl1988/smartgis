// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_VIEWPORT_VIEW_2D_H_
#define LEGACY_UI_MAP_VIEWPORT_VIEW_2D_H_

#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "gis/map/map.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "legacy/render/rhi2d/public/device/renderer.h"
#include "legacy/tool/factory/grouptoolfactory.h"
#include "legacy/ui/map/framing/oper_map_frame.h"
#include "legacy/ui/shell/xview.h"

using namespace render;
using namespace tool;
using namespace gis;

namespace ui {

// Leftover 2D map CView host: GDI+ present, Workspace tools, oper-map framing.
class XVIEW_EXPORT Smt2DXView : public SmtXView {
  DECLARE_DYNCREATE(Smt2DXView)

 public:
  Smt2DXView();
  virtual ~Smt2DXView();

  LPRENDERDEVICE GetRenderDevice(void);

  virtual void OnDraw(CDC* pDC);
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
  afx_msg LRESULT OnDeferredContextMenu(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT OnFrameOperMap(WPARAM wParam, LPARAM lParam);

  virtual void SetOperMap(Map* pSmtMap);
  Map* GetOperMap(void);

 protected:
  bool InitCreate(void);
  bool EndDestory(void);
  bool CreateContexMenu();
  bool CreateMainMenu(void);

  bool CreateRender(void);
  bool CreateTools(void);
  void apply_workspace_draft(const tool::Draft& draft) override;

  bool frame_oper_map(bool realtime);
  void request_oper_map_frame();

 protected:
  UINT m_uiNotifyTimer;
  UINT m_uiRefreshTimer;

  LPRENDERER m_pRenderer;
  LPRENDERDEVICE m_pRenderDevice;

  SmtBaseTool* m_pViewCtrlTool;
  SmtBaseTool* m_pSelectTool;
  SmtBaseTool* m_pFlashTool;

  Map* m_pSmtOperMap;
  detail::OperMapFrameState m_oper_map_frame;
};

}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // LEGACY_UI_MAP_VIEWPORT_VIEW_2D_H_
