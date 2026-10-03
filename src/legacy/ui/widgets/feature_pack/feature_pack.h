// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_WIDGETS_FEATURE_PACK_H_
#define LEGACY_UI_WIDGETS_FEATURE_PACK_H_

// Mechanical BCGControlBar Pro → MFC Feature Pack mapping for the leftover
// 2010 desktop shell. Feature Pack (CMFC* / CDockablePane / CFrameWndEx) is
// the Microsoft-licensed descendant; BCG is not vendored.
//
// Call sites keep historical CBCGP* / BCGP_* / BCGM_* names via typedefs and
// macros below. New leftover code should prefer the CMFC* names directly.

#include <afxcontrolbars.h>

typedef CMDIFrameWndEx CBCGPMDIFrameWnd;
typedef CMDIChildWndEx CBCGPMDIChildWnd;
typedef CDockablePane CBCGPDockingControlBar;
typedef CMFCMenuBar CBCGPMenuBar;
typedef CMFCToolBar CBCGPToolBar;
typedef CMFCStatusBar CBCGPStatusBar;
typedef CMFCTabCtrl CBCGPTabWnd;
typedef CMFCOutlookBar CBCGPOutlookBar;
typedef CMFCOutlookBarTabCtrl CBCGPOutlookWnd;
typedef CMFCPropertyGridCtrl CBCGPPropList;
typedef CMFCPropertyGridProperty CBCGPProp;
typedef CMFCPropertyGridFontProperty CBCGPFontProp;
typedef CMFCPropertyGridColorProperty CBCGPColorProp;
typedef CMFCVisualManager CBCGPVisualManager;
typedef CMFCVisualManagerOffice2003 CBCGPVisualManager2003;
typedef CMFCVisualManagerOffice2007 CBCGPVisualManager2007;
typedef CDockingManager CBCGPDockManager;
typedef CMDITabInfo CBCGPMDITabParams;
typedef CMFCToolTipInfo CBCGPToolTipParams;
typedef CMFCToolTipCtrl CBCGPToolTipCtrl;
typedef CMFCTabToolTipInfo CBCGPTabToolTipInfo;

#ifndef CBRS_BCGP_FLOAT
#define CBRS_BCGP_FLOAT AFX_CBRS_FLOAT
#endif
#ifndef CBRS_BCGP_AUTOHIDE
#define CBRS_BCGP_AUTOHIDE AFX_CBRS_AUTOHIDE
#endif
#ifndef CBRS_BCGP_RESIZE
#define CBRS_BCGP_RESIZE AFX_CBRS_RESIZE
#endif
#ifndef CBRS_BCGP_CLOSE
#define CBRS_BCGP_CLOSE AFX_CBRS_CLOSE
#endif
// Tabbed CDockablePane hosts (Catalog) must use regular Feature Pack tabs.
#ifndef CBRS_BCGP_REGULAR_TABS
#define CBRS_BCGP_REGULAR_TABS AFX_CBRS_REGULAR_TABS
#endif
// Outlook-style panes (CMFCOutlookBar / AMBox) keep the Outlook tab style.
#ifndef CBRS_BCGP_OUTLOOK_TABS
#define CBRS_BCGP_OUTLOOK_TABS AFX_CBRS_OUTLOOK_TABS
#endif

#ifndef BCGP_DT_SMART
#ifdef DT_SMART
#define BCGP_DT_SMART DT_SMART
#else
#define BCGP_DT_SMART 0x02
#endif
#endif

#ifndef BCGP_TOOLTIP_TYPE_ALL
#ifdef AFX_TOOLTIP_TYPE_ALL
#define BCGP_TOOLTIP_TYPE_ALL AFX_TOOLTIP_TYPE_ALL
#else
#define BCGP_TOOLTIP_TYPE_ALL 0xFFFF
#endif
#endif

#ifndef BCGM_PROPERTY_CHANGED
#define BCGM_PROPERTY_CHANGED AFX_WM_PROPERTY_CHANGED
#endif
#ifndef BCGM_ON_GET_TAB_TOOLTIP
#ifdef AFX_WM_ON_GET_TAB_TOOLTIPS
#define BCGM_ON_GET_TAB_TOOLTIP AFX_WM_ON_GET_TAB_TOOLTIPS
#else
#define BCGM_ON_GET_TAB_TOOLTIP AFX_WM_ON_GET_TAB_TOOLTIP
#endif
#endif

inline void BCGCBProCleanUp() {}

// BCG exposes a global `globalData` object, not a namespace.
//
// DPI policy for leftover MFC SmartGis.exe (industry visual-test baseline):
// stay process DPI-unaware. Dialog templates, dock metrics, and Feature Pack
// shell are authored in physical pixels (2010-era). Calling
// SetProcessDPIAware() without Per-Monitor V2 layout scaling clips captions
// ("ataloc"), misaligns menu/dock bars, and mis-centers modal dialogs on
// high-DPI displays. Windows then bitmap-scales the whole UI coherently.
// Newer hosts (Views / WinUI) opt into PerMonitorV2 via their own manifests.
struct BcgGlobalData {
  void SetDPIAware() {
    // Intentionally a no-op: do not call ::SetProcessDPIAware().
  }
};
inline BcgGlobalData globalData;

#endif  // LEGACY_UI_WIDGETS_FEATURE_PACK_H_
