// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_XVIEW_H_
#define LEGACY_UI_SHELL_XVIEW_H_

#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif

#include "legacy/core/macros/macros.h"
#include "legacy/core/listener/listener_manager.h"
#include "tool/draft/draft.h"
using namespace base;

namespace content {
class ViewHost;
}

#define CPtTolPt(cpt) (lPoint(cpt.x, cpt.y))

namespace ui {

// MFC CView chrome base for leftover map/3D hosts; routes input via ViewHost.
class XVIEW_EXPORT SmtXView : public CView, public SmtListener {
  DECLARE_DYNCREATE(SmtXView)

 protected:
  SmtXView();
  virtual ~SmtXView();

 public:
  long BindWind(HWND hWnd);
  long BindDlgItem(CDialog* pDlg, UINT nItemID);
  long UnbindWind(void);

 public:
  virtual void OnInitialUpdate();
  virtual void OnDraw(CDC* pDC);
  virtual void OnActivateView(BOOL bActivate, CView* pActivateView,
                              CView* pDeactiveView);
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
  virtual void PostNcDestroy();
  virtual void OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame);

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

 public:
  virtual bool InitCreate(void);
  virtual bool EndDestory(void);

  virtual bool CreateMainMenu(void);
  virtual bool CreateContexMenu(void);

  virtual bool CreateRender(void) { return true; }
  virtual bool CreateTools(void) { return true; }

  content::ViewHost* view_host() { return m_pViewHost; }
  void reset_view_host(content::ViewHost* host);
  bool route_shell_command(unsigned int msg);
  void bind_draft_observer();
  void dispatch_menu_command(unsigned int msg);
  virtual void apply_workspace_draft(const tool::Draft& draft);

 public:
  virtual int notify(long nMsg, SmtListenerMsg& param);

 protected:
  HMENU m_hContexMenu;
  HMENU m_hMainMenu;

  BOOL m_bActive;
  content::ViewHost* m_pViewHost;

 protected:
  CDialog* m_pBindDlg;
  UINT m_unBindItemID;
};
}  // namespace ui

#if !defined(XVIEW_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // LEGACY_UI_SHELL_XVIEW_H_
