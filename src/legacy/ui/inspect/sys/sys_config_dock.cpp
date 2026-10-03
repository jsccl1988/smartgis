// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/inspect/sys/sys_config_dock.h"

#include "legacy/gis/present/carto/style_api.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/ui/inspect/host/prop_host.h"
#include "legacy/ui/inspect/sys/flash_styles.h"

using namespace sys;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace {

const char* const k_true_or_false[] = {"False", "True", nullptr};

}  // namespace

BEGIN_MESSAGE_MAP(SysConfigDockBar, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_CONTEXTMENU()
ON_REGISTERED_MESSAGE(BCGM_PROPERTY_CHANGED, OnPropertyChanged)
END_MESSAGE_MAP()

SysConfigDockBar::SysConfigDockBar() = default;

SysConfigDockBar::~SysConfigDockBar() = default;

BOOL SysConfigDockBar::Create(CWnd* parent, UINT id) {
  return legacy_ui::create_prop_host_child(this, parent, id);
}

int SysConfigDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CWnd::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  if (!legacy_ui::create_vs_prop_list(m_wndPropList, this, 1)) {
    TRACE0("Failed to create Properies Grid \n");
    return -1;
  }

  if (!CreateProList()) {
    return -1;
  }

  return 0;
}

void SysConfigDockBar::OnSize(UINT nType, int cx, int cy) {
  CWnd::OnSize(nType, cx, cy);
  legacy_ui::size_prop_host(m_wndPropList, cx, cy);
}

BOOL SysConfigDockBar::OnEraseBkgnd(CDC* pDC) {
  return legacy_ui::erase_prop_host_bkgnd(this, pDC);
}

void SysConfigDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/) {}

void SysConfigDockBar::OnPaint() {
  legacy_ui::paint_prop_host_border(this, m_wndPropList);
}

bool SysConfigDockBar::CreateProList() {
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  m_wndPropList.AddProperty(
      new CBCGPColorProp(_T("Flash Color1"), sysPra.flashPra.lClr1, NULL,
                         _T("Flash Color1"), PRO_FLASH_CLR1));
  m_wndPropList.AddProperty(
      new CBCGPColorProp(_T("Flash Color2"), sysPra.flashPra.lClr2, NULL,
                         _T("Flash Color2"), PRO_FLASH_CLR2));
  m_wndPropList.AddProperty(
      new CBCGPProp(_T("Flash Elapse"), (_variant_t)(sysPra.flashPra.lElapse),
                    _T("Flash Elapse"), PRO_FLASH_ELAPSE));

  m_wndPropList.AddProperty(
      new CBCGPProp(_T("SMargin"), (_variant_t)(sysPra.fSmargin),
                    _T("Flash Elapse"), PRO_2DVIEW_SMARGIN));
  m_wndPropList.AddProperty(
      new CBCGPProp(_T("2DView Zoom Scale Delt"), (_variant_t)(sysPra.fSmargin),
                    _T("2DView Zoom Scale Delt"), PRO_2DVIEW_ZOOMSCALEDELT));

  {
    int index = sysPra.bShowMBR ? 1 : 0;
    CBCGPProp* pProp =
        new CBCGPProp(_T("Show MBR"), (_variant_t)(k_true_or_false[index]),
                      _T("Show MBR"), PRO_2DVIEW_SHOWMBR);
    legacy_ui::add_prop_options(pProp, k_true_or_false);
    m_wndPropList.AddProperty(pProp);
  }

  {
    int index = sysPra.bShowPoint ? 1 : 0;
    CBCGPProp* pProp =
        new CBCGPProp(_T("Show Point"), (_variant_t)(k_true_or_false[index]),
                      _T("Show Point"), PRO_2DVIEW_SHOWPOINT);
    legacy_ui::add_prop_options(pProp, k_true_or_false);
    m_wndPropList.AddProperty(pProp);
  }

  m_wndPropList.AddProperty(
      new CBCGPProp(_T("Point Radius"), (_variant_t)(sysPra.lPointRaduis),
                    _T("Point Radius"), PRO_2DVIEW_POINTRADUIS));

  m_wndPropList.AddProperty(
      new CBCGPColorProp(_T("3D Back Color"), sysPra.l3DViewClearColor, NULL,
                         _T("3D Back Color"), PRO_3DVIEW_CLEARCOLOR));

  SetPropState();

  return true;
}

LRESULT SysConfigDockBar::OnPropertyChanged(WPARAM, LPARAM lParam) {
  CBCGPProp* pProp = (CBCGPProp*)lParam;

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();
  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();
  SmtSysPra sysPra = pSysMgr->get_sys_pra();
  const auto flash =
      legacy_ui::detail::load_flash_styles(pStyleMgr, styleConfig);

  switch ((int)pProp->GetData()) {
    case PRO_FLASH_CLR1: {
      CBCGPColorProp* pClrProp = (CBCGPColorProp*)pProp;
      sysPra.flashPra.lClr1 = pClrProp->GetColor();
      legacy_ui::detail::apply_flash_color1(flash, sysPra.flashPra.lClr1);
    } break;
    case PRO_FLASH_CLR2: {
      CBCGPColorProp* pClrProp = (CBCGPColorProp*)pProp;
      sysPra.flashPra.lClr2 = pClrProp->GetColor();
      legacy_ui::detail::apply_flash_color2(flash, sysPra.flashPra.lClr2);
    } break;
    case PRO_FLASH_ELAPSE: {
      sysPra.flashPra.lElapse = legacy_ui::prop_as_long(pProp->GetValue());
    } break;
    case PRO_2DVIEW_SMARGIN: {
      sysPra.fSmargin = legacy_ui::prop_as_float(pProp->GetValue());
    } break;
    case PRO_2DVIEW_ZOOMSCALEDELT: {
      sysPra.fZoomScaleDelt = legacy_ui::prop_as_float(pProp->GetValue());
    } break;
    case PRO_2DVIEW_SHOWMBR: {
      CString strStyle = (LPCTSTR)(_bstr_t)pProp->GetValue();
      const int index = legacy_ui::option_index(strStyle, k_true_or_false);
      if (index >= 0) {
        sysPra.bShowMBR = index;
      }
    } break;
    case PRO_2DVIEW_SHOWPOINT: {
      CString strStyle = (LPCTSTR)(_bstr_t)pProp->GetValue();
      const int index = legacy_ui::option_index(strStyle, k_true_or_false);
      if (index >= 0) {
        sysPra.bShowPoint = index;
      }
    } break;
    case PRO_2DVIEW_POINTRADUIS: {
      sysPra.lPointRaduis = legacy_ui::prop_as_long(pProp->GetValue());
    } break;
    case PRO_3DVIEW_CLEARCOLOR: {
      sysPra.l3DViewClearColor = legacy_ui::prop_as_long(pProp->GetValue());
    } break;
  }

  pSysMgr->set_sys_pra(sysPra);

  return NULL;
}

void SysConfigDockBar::SetPropState() {
  if (m_wndPropList.GetSafeHwnd() != NULL) {
    m_wndPropList.RedrawWindow();
  }
}
