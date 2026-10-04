// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_MDI_TAB_OPTIONS_H_
#define LEGACY_APP_SHELL_MDI_TAB_OPTIONS_H_

#pragma once

// Persisted MDI tab chrome options for leftover CSmartGisApp.
class CMDITabOptions {
 public:
  CMDITabOptions();

  enum MDITabsType { None, MDITabsStandard, MDITabbedGroups };

  void Load();
  void Save();

  BOOL IsMDITabsDisabled() const {
    return m_nMDITabsType == CMDITabOptions::None;
  }

  MDITabsType m_nMDITabsType = MDITabsStandard;
  BOOL m_bMaximizeMDIChild = TRUE;
  BOOL m_bTabsOnTop = TRUE;
  BOOL m_bActiveTabCloseButton = TRUE;
  CBCGPTabWnd::Style m_nTabsStyle = CBCGPTabWnd::STYLE_FLAT;
  BOOL m_bTabsAutoColor = FALSE;
  BOOL m_bMDITabsIcons = FALSE;
  BOOL m_bMDITabsDocMenu = FALSE;
  BOOL m_bDragMDITabs = TRUE;
  BOOL m_bMDITabsContextMenu = TRUE;
  int m_nMDITabsBorderSize = 1;
  BOOL m_bDisableMDIChildRedraw = TRUE;
  BOOL m_bFlatFrame = TRUE;
  BOOL m_bCustomTooltips = FALSE;
};

#endif  // LEGACY_APP_SHELL_MDI_TAB_OPTIONS_H_
