
#if !defined(AFX_CHILDFRM_H__E65A14C6_B44E_4656_92AE_7FCD67ED2561__INCLUDED_)
#define AFX_CHILDFRM_H__E65A14C6_B44E_4656_92AE_7FCD67ED2561__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#define CChildWnd CBCGPMDIChildWnd

class CChildFrame : public CChildWnd {
  DECLARE_DYNCREATE(CChildFrame)
 public:
  CChildFrame();

 public:
  virtual ~CChildFrame();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  DECLARE_MESSAGE_MAP()
  virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                      DWORD dwStyle = WS_CHILD | WS_VISIBLE |
                                      WS_OVERLAPPEDWINDOW,
                      const RECT& rect = rectDefault,
                      CMDIFrameWnd* pParentWnd = NULL,
                      CCreateContext* pContext = NULL);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  // Keep per-view titles (Scene3D) — shared Edit doc must not rename tabs.
  virtual void OnUpdateFrameTitle(BOOL bAddToTitle);

 public:
  virtual void ActivateFrame(int nCmdShow = -1);
  virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
};

#endif  // !defined(AFX_CHILDFRM_H__E65A14C6_B44E_4656_92AE_7FCD67ED2561__INCLUDED_)
