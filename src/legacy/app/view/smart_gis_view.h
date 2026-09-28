// TODO(sp3): Prefer deleting with the leftover exe; product paint is MapScene.

#pragma once

class CSmartGisDoc;
class CSmartGisView : public CView {
 protected:
  CSmartGisView();
  DECLARE_DYNCREATE(CSmartGisView)

 public:
  CSmartGisDoc* GetDocument() const;

 public:
  virtual void OnDraw(CDC* pDC);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

 protected:
  virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
  virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
  virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

 public:
  virtual ~CSmartGisView();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  DECLARE_MESSAGE_MAP()

 public:
  virtual void OnInitialUpdate();
  virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg void OnSize(UINT nType, int cx, int cy);

  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
};

#ifndef _DEBUG
inline CSmartGisDoc* CSmartGisView::GetDocument() const {
  return reinterpret_cast<CSmartGisDoc*>(m_pDocument);
}
#endif
