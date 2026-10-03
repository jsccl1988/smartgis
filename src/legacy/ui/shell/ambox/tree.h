// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_AMBOX_TREE_H_
#define LEGACY_UI_SHELL_AMBOX_TREE_H_

#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif

#include "legacy/core/macros/macros.h"
#include "legacy/plugin/runtime/auxmodule/module.h"
#include "legacy/ui/shell/ambox/title.h"

using namespace plugin;

namespace ui {

// Per-AuxModule tree hosted on one Outlook page.
class XAMBOX_EXPORT SmtXAMBox : public CTreeCtrl {
  DECLARE_DYNAMIC(SmtXAMBox)

 public:
  SmtXAMBox(SmtAuxModule* pAModule);
  virtual ~SmtXAMBox();

 public:
  virtual bool InitCreate(void);
  virtual bool EndDestory(void);
  virtual bool CreateContexMenu(void);

 public:
  virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                      UINT nID);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

 public:
  bool UpdateAMBoxTree(void);

 protected:
  DECLARE_MESSAGE_MAP()

 protected:
  HMENU m_hContexMenu;

 protected:
  CImageList m_imgList;
  HTREEITEM m_hRoot;

  vSmtFuncItems m_vFuncItems;
  SmtAuxModule* m_pAModule;
};
}  // namespace ui

#if !defined(XAMBOX_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // LEGACY_UI_SHELL_AMBOX_TREE_H_
