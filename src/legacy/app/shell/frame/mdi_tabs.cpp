// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/shell/frame/mdi_tabs.h"

#include "legacy/app/shell/frame/win_app.h"

CMDITabOptions::CMDITabOptions() {
  // Flat top tabs + no per-doc icons — closer to Views TabStrip (Map/Data/3D).
  m_nMDITabsType = CMDITabOptions::MDITabsStandard;
  m_bMaximizeMDIChild = TRUE;
  m_bTabsOnTop = TRUE;
  m_bActiveTabCloseButton = TRUE;
  m_nTabsStyle = CBCGPTabWnd::STYLE_FLAT;
  m_bTabsAutoColor = FALSE;
  m_bMDITabsIcons = FALSE;
  m_bMDITabsDocMenu = FALSE;
  m_bDragMDITabs = TRUE;
  m_bMDITabsContextMenu = TRUE;
  m_nMDITabsBorderSize = 1;
  m_bDisableMDIChildRedraw = TRUE;
  m_bFlatFrame = TRUE;
  m_bCustomTooltips = FALSE;
}

void CMDITabOptions::Load() {
  m_nMDITabsType = (MDITabsType)theApp.GetInt(_T("ShowMDITabs"), TRUE);
  m_bMaximizeMDIChild = theApp.GetInt(_T("MaximizeMDIChild"), TRUE);
  m_bTabsOnTop = theApp.GetInt(_T("TabsOnTop"), TRUE);
  m_bActiveTabCloseButton = theApp.GetInt(_T("ActiveTabCloseButton"), TRUE);
  m_nTabsStyle = (CBCGPTabWnd::Style)theApp.GetInt(
      _T("TabsStyle"), CBCGPTabWnd::STYLE_FLAT);
  m_bTabsAutoColor = theApp.GetInt(_T("TabsAutoColor"), FALSE);
  m_bMDITabsIcons = theApp.GetInt(_T("MDITabsIcons"), FALSE);
  m_bMDITabsDocMenu = theApp.GetInt(_T("MDITabsDocMenu"), FALSE);
  m_bDragMDITabs = theApp.GetInt(_T("DragMDITabs"), TRUE);
  m_bMDITabsContextMenu = theApp.GetInt(_T("MDITabsContextMenu"), TRUE);
  m_nMDITabsBorderSize = theApp.GetInt(_T("MDITabsBorderSize"), 1);
  m_bDisableMDIChildRedraw = theApp.GetInt(_T("DisableMDIChildRedraw"), TRUE);
  m_bFlatFrame = theApp.GetInt(_T("FlatFrame"), TRUE);
  m_bCustomTooltips = theApp.GetInt(_T("CustomTooltips"), FALSE);
}

void CMDITabOptions::Save() {
  theApp.WriteInt(_T("ShowMDITabs"), m_nMDITabsType);
  theApp.WriteInt(_T("MaximizeMDIChild"), m_bMaximizeMDIChild);
  theApp.WriteInt(_T("TabsOnTop"), m_bTabsOnTop);
  theApp.WriteInt(_T("ActiveTabCloseButton"), m_bActiveTabCloseButton);
  theApp.WriteInt(_T("TabsStyle"), m_nTabsStyle);
  theApp.WriteInt(_T("TabsAutoColor"), m_bTabsAutoColor);
  theApp.WriteInt(_T("MDITabsIcons"), m_bMDITabsIcons);
  theApp.WriteInt(_T("MDITabsDocMenu"), m_bMDITabsDocMenu);
  theApp.WriteInt(_T("DragMDITabs"), m_bDragMDITabs);
  theApp.WriteInt(_T("MDITabsContextMenu"), m_bMDITabsContextMenu);
  theApp.WriteInt(_T("MDITabsBorderSize"), m_nMDITabsBorderSize);
  theApp.WriteInt(_T("DisableMDIChildRedraw"), m_bDisableMDIChildRedraw);
  theApp.WriteInt(_T("FlatFrame"), m_bFlatFrame);
  theApp.WriteInt(_T("CustomTooltips"), m_bCustomTooltips);
}
