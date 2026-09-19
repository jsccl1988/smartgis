#ifndef _STACKEDWND_DOCKBAR_H
#define _STACKEDWND_DOCKBAR_H
#if defined(MFC_EX_EXPORTS)
#define MFC_EX_EXPORT __declspec(dllexport)
#else
#define MFC_EX_EXPORT __declspec(dllimport)
#endif

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#include "base/core/core.h"

class AFX_EXT_CLASS StackedWndDockBar : public CBCGPOutlookBar {
 public:
  StackedWndDockBar();
  virtual ~StackedWndDockBar();

 public:
  bool AddWnd(CWnd* pWnd, CString strTitle);
  CBCGPOutlookWnd* GetOnerWnd(void) {
    return DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());
  }

 protected:
  afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

  DECLARE_MESSAGE_MAP()

 protected:
  vector<CWnd*> m_vWndPtrs;
  int m_nToolBoxPage;
};

#if !defined(MFC_EX_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_STACKEDWND_DOCKBAR_H
