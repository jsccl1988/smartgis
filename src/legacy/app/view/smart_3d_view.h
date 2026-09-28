#pragma once

// Thin MFC forwarder over Smt3DXView.
// TODO(sp3): 3D present path strangler is SP4 / app::Scene3dController; leave
// this as an HWND shell until leftover GL create is retired.
#include "legacy/ui/xview/view_3d.h"
using namespace ui;

class CSmartGisDoc;
class CSmart3DView : public Smt3DXView {
  DECLARE_DYNCREATE(CSmart3DView)

 protected:
  CSmart3DView();
  virtual ~CSmart3DView();

 public:
  CSmartGisDoc* GetDocument() const;

 public:
  virtual void OnInitialUpdate();
  virtual void OnDraw(CDC* pDC);

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext& dc) const;
#endif
#endif

 public:
  DECLARE_MESSAGE_MAP()
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);

 public:
  virtual int Notify(long nMsg, SmtListenerMsg& param);
};

#ifndef _DEBUG
inline CSmartGisDoc* CSmart3DView::GetDocument() const {
  return reinterpret_cast<CSmartGisDoc*>(m_pDocument);
}
#endif
