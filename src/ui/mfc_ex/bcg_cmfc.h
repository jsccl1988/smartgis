// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_BCG_CMFC_H
#define SMT_BCG_CMFC_H

// Mechanical BCGControlBar Pro → MFC Feature Pack mapping for the 2010
// desktop shell. Feature Pack (CMFC* / CDockablePane / CFrameWndEx) is the
// Microsoft-licensed descendant; BCG is not vendored.

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
#ifndef CBRS_BCGP_OUTLOOK_TABS
#ifdef AFX_CBRS_OUTLOOK_TABS
#define CBRS_BCGP_OUTLOOK_TABS AFX_CBRS_OUTLOOK_TABS
#else
#define CBRS_BCGP_OUTLOOK_TABS AFX_CBRS_REGULAR_TABS
#endif
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

// Feature Pack GetValue() is const COleVariant&; BCG allowed numeric C-casts.
inline long bcg_prop_as_long(const COleVariant& v) {
  COleVariant tmp(v);
  tmp.ChangeType(VT_I4);
  return tmp.lVal;
}

inline float bcg_prop_as_float(const COleVariant& v) {
  COleVariant tmp(v);
  tmp.ChangeType(VT_R4);
  return tmp.fltVal;
}

inline void BCGCBProCleanUp() {}

// BCG exposes a global `globalData` object, not a namespace.
struct BcgGlobalData {
  void SetDPIAware() { ::SetProcessDPIAware(); }
};
inline BcgGlobalData globalData;

#endif
