#pragma once

// Thin MFC forwarder over Smt2DXView (data-source catalog paint).
// TODO(sp3): Catalog snapshot JSON already lives in content::catalog_layers;
// retire this view once Views Catalog sync covers leftover open paths.
#include "legacy/ui/map/view_2d.h"
using namespace ui;

class CSmartGisDoc;
class CSmartDataSourceView : public Smt2DXView {
  DECLARE_DYNCREATE(CSmartDataSourceView)

protected:
  CSmartDataSourceView();
  virtual ~CSmartDataSourceView();

public:
  CSmartGisDoc *GetDocument() const;

public:
  virtual void OnInitialUpdate();
  virtual void OnDraw(CDC *pDC);

#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext &dc) const;
#endif
#endif

protected:
  DECLARE_MESSAGE_MAP()

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();

public:
  virtual int Notify(long nMsg, SmtListenerMsg &param);
};
#ifndef _DEBUG
inline CSmartGisDoc *CSmartDataSourceView::GetDocument() const {
  return reinterpret_cast<CSmartGisDoc *>(m_pDocument);
}
#endif
