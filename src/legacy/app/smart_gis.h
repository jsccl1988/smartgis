// smart_gis.h : SmartGis Ӧ�ó������ͷ�ļ�
//
#pragma once

#ifndef __AFXWIN_H__
#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy/app/resource.h"  // ������

// CSmartGisApp:
// �йش����ʵ�֣������ smart_gis.cpp
//

#include "base/core/core.h"
#include "base/core/env_struct.h"
#include "legacy/app/smtapp.h"

using namespace base;
using namespace app;

class CMDITabOptions {
 public:
  CMDITabOptions();

  enum MDITabsType { None, MDITabsStandard, MDITabbedGroups };

 public:
  void Load();
  void Save();

  BOOL IsMDITabsDisabled() const {
    return m_nMDITabsType == CMDITabOptions::None;
  }

 public:
  MDITabsType m_nMDITabsType;
  BOOL m_bMaximizeMDIChild;
  BOOL m_bTabsOnTop;
  BOOL m_bActiveTabCloseButton;
  CBCGPTabWnd::Style m_nTabsStyle;
  BOOL m_bTabsAutoColor;
  BOOL m_bMDITabsIcons;
  BOOL m_bMDITabsDocMenu;
  BOOL m_bDragMDITabs;
  BOOL m_bMDITabsContextMenu;
  int m_nMDITabsBorderSize;
  BOOL m_bDisableMDIChildRedraw;
  BOOL m_bFlatFrame;
  BOOL m_bCustomTooltips;
};

class CSmartGisApp : public CWinAppEx, public SmtApp {
 public:
  CSmartGisApp();

  // ��д
 public:
  virtual BOOL InitInstance();
  virtual int ExitInstance();

  virtual void PreLoadState();

  // ʵ��
  afx_msg void OnAppAbout();
  DECLARE_MESSAGE_MAP()

 public:
  //////////////////////////////////////////////////////////////////////////
  //
  inline CMultiDocTemplate* GetEditViewDocTemplate() {
    return m_pEditViewDocTemplate;
  }
  inline CMultiDocTemplate* GetDataViewDocTemplate() {
    return m_pDataViewDocTemplate;
  }
  inline CMultiDocTemplate* Get3DViewDocTemplate() {
    return m_p3DViewDocTemplate;
  }

  // Window popup on the dynamic view menu (RC Window menu is replaced).
  void append_mdi_window_menu(HMENU menu);
  // Open another MDI view on the active document, or a new doc if none.
  BOOL open_mdi_view(CDocTemplate* tmpl);

  //////////////////////////////////////////////////////////////////////////
  CView* GetActiveDocView(CRuntimeClass* pViewClass);
  CView* GetActiveView(void);
  CDocument* GetActiveDoc(void);

 public:
  CMDITabOptions m_Options;

 protected:
  CMultiDocTemplate* m_pEditViewDocTemplate;
  CMultiDocTemplate* m_pDataViewDocTemplate;
  CMultiDocTemplate* m_p3DViewDocTemplate;
};

extern CSmartGisApp theApp;