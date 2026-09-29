#pragma once

// Thin MFC forwarder over Smt2DEditXView (leftover HWND paint / Notify).
// TODO(sp3): Viewport + command wiring belongs in src/app/views (MapScene /
// ViewHost). Keep this class as a CView shell until leftover paint is retired.
#include "legacy/ui/map/view_2d_edit.h"
using namespace ui;

class CSmartGisDoc;
class CSmartMapEditView : public Smt2DEditXView {
  DECLARE_DYNCREATE(CSmartMapEditView)

 protected:
  CSmartMapEditView();
  virtual ~CSmartMapEditView();

 public:
  CSmartGisDoc* GetDocument() const;

 public:
  virtual void OnDraw(CDC* pDC);
  virtual void OnInitialUpdate();

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 protected:
  DECLARE_MESSAGE_MAP()
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);

  afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
  afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);

 public:
  virtual int Notify(long nMsg, SmtListenerMsg& param);
};

#ifndef _DEBUG
inline CSmartGisDoc* CSmartMapEditView::GetDocument() const {
  return reinterpret_cast<CSmartGisDoc*>(m_pDocument);
}
#endif
