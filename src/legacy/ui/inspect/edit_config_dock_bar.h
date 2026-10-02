// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _EDITCONFIGDOCKBAR_H
#define _EDITCONFIGDOCKBAR_H
#if defined(GUI_EXPORTS)
#define GUI_EXPORT __declspec(dllexport)
#else
#define GUI_EXPORT __declspec(dllimport)
#endif

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#include "legacy/core/macros/macros.h"

// Plain CWnd page for AMBox Outlook. Must NOT derive from
// CBCGPDockingControlBar — nested docking bars inside Outlook crash on close.
class AFX_EXT_CLASS EditConfigDockBar : public CWnd {
 public:
  enum {
    PRO_TEXT_Font,
    PRO_ChildImage_ID,
    PRO_ChildImage_Width,
    PRO_ChildImage_Height,
    PRO_Line_Color,
    PRO_Line_Style,
    PRO_Line_Width,
    PRO_Reg_Color,
    PRO_Reg_FillStyle,
    PRO_Reg_HatchStyle,
  };

  EditConfigDockBar();
  ~EditConfigDockBar() override;

  BOOL Create(CWnd* parent, UINT id);

 public:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnPaint();
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);

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

#endif  //_EDITCONFIGDOCKBAR_H
