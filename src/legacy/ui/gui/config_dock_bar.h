#ifndef _SYSCONFIGDOCKBAR_H
#define _SYSCONFIGDOCKBAR_H
#if defined(GUI_EXPORTS)
#define GUI_EXPORT __declspec(dllexport)
#else
#define GUI_EXPORT __declspec(dllimport)
#endif

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#include "base/core/core.h"

class AFX_EXT_CLASS SysConfigDockBar : public CBCGPDockingControlBar {
 public:
  enum {
    PRO_FLASH_CLR1,
    PRO_FLASH_CLR2,
    PRO_FLASH_ELAPSE,
    PRO_2DVIEW_SMARGIN,
    PRO_2DVIEW_ZOOMSCALEDELT,
    PRO_2DVIEW_SHOWMBR,
    PRO_2DVIEW_SHOWPOINT,
    PRO_2DVIEW_POINTRADUIS,
    PRO_2DVIEW_REFRESHTIME,
    PRO_2DVIEW_NOTIFYTIME,
    PRO_3DVIEW_CLEARCOLOR,
    PRO_3DVIEW_REFRESHTIME,
    PRO_3DVIEW_NOTIFYTIME,
  };

  SysConfigDockBar();
  virtual ~SysConfigDockBar();

 public:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnPaint();

  afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
  afx_msg LRESULT OnPropertyChanged(WPARAM, LPARAM);

 protected:
  bool CreateProList();
  void SetPropState();

  DECLARE_MESSAGE_MAP()

 protected:
  CBCGPPropList m_wndPropList;
};

#if !defined(GUI_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  //_SYSCONFIGDOCKBAR_H
