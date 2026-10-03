// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/inspect/edit/edit_config_dock.h"

#include "legacy/gis/present/carto/style_api.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/ui/inspect/host/prop_host.h"

using namespace sys;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace {

const char* const k_line_styles[] = {"SOLID",   "-------", ".......",
                                     "_._._._", "_.._.._", nullptr};

const char* const k_reg_fill_styles[] = {"SOLID", "Hatch", nullptr};

const char* const k_reg_hatch_styles[] = {
    "-----", "|||||", "\\\\\\\\\\", "/////", "+++++", "xxxxx", nullptr};

}  // namespace

BEGIN_MESSAGE_MAP(EditConfigDockBar, CWnd)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_CONTEXTMENU()
ON_REGISTERED_MESSAGE(BCGM_PROPERTY_CHANGED, OnPropertyChanged)
END_MESSAGE_MAP()

EditConfigDockBar::EditConfigDockBar() = default;

EditConfigDockBar::~EditConfigDockBar() = default;

BOOL EditConfigDockBar::Create(CWnd* parent, UINT id) {
  return legacy_ui::create_prop_host_child(this, parent, id);
}

int EditConfigDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
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

void EditConfigDockBar::OnSize(UINT nType, int cx, int cy) {
  CWnd::OnSize(nType, cx, cy);
  legacy_ui::size_prop_host(m_wndPropList, cx, cy);
}

BOOL EditConfigDockBar::OnEraseBkgnd(CDC* pDC) {
  return legacy_ui::erase_prop_host_bkgnd(this, pDC);
}

void EditConfigDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/) {}

void EditConfigDockBar::OnPaint() {
  legacy_ui::paint_prop_host_border(this, m_wndPropList);
}

bool EditConfigDockBar::CreateProList() {
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();
  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();

  SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
  if (pStyle) {
    LOGFONT lgFont;
    anno_desc_to_log_font(lgFont, pStyle->get_anno_desc());
    m_wndPropList.AddProperty(
        new CBCGPFontProp(_T("Anno Font"), lgFont, CF_EFFECTS | CF_SCREENFONTS,
                          _T("Anno Font"), PRO_TEXT_Font));
  }

  pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
  if (pStyle) {
    SmtSymbolDesc stSymbolDes = pStyle->get_symbol_desc();
    m_wndPropList.AddProperty(
        new CBCGPProp(_T("Symble-ID"), (_variant_t)(stSymbolDes.lSymbolID),
                      _T("Child Image ID"), PRO_ChildImage_ID));
    m_wndPropList.AddProperty(new CBCGPProp(
        _T("Symble-Height"), (_variant_t)(stSymbolDes.fSymbolHeight),
        _T("Child Image Height"), PRO_ChildImage_Height));
    m_wndPropList.AddProperty(new CBCGPProp(
        _T("Symble-Width"), (_variant_t)(stSymbolDes.fSymbolWidth),
        _T("Child Image Width"), PRO_ChildImage_Width));
  }

  pStyle = pStyleMgr->get_style(styleConfig.szLineStyle);
  if (pStyle) {
    SmtPenDesc stPenDes = pStyle->get_pen_desc();
    m_wndPropList.AddProperty(
        new CBCGPColorProp(_T("Line Color"), stPenDes.lPenColor, NULL,
                           _T("Line Color"), PRO_Line_Color));

    CBCGPProp* pProp =
        new CBCGPProp(_T("Line Style"), (_variant_t)(k_line_styles[0]),
                      _T("Line Style"), PRO_Line_Style);
    legacy_ui::add_prop_options(pProp, k_line_styles);
    m_wndPropList.AddProperty(pProp);

    m_wndPropList.AddProperty(new CBCGPProp(_T("Line Width"),
                                            (_variant_t)(stPenDes.fPenWidth),
                                            _T("Line Width"), PRO_Line_Width));
  }

  pStyle = pStyleMgr->get_style(styleConfig.szRegionStyle);
  if (pStyle) {
    SmtBrushDesc stBrushDes = pStyle->get_brush_desc();

    m_wndPropList.AddProperty(
        new CBCGPColorProp(_T("Reg Color"), stBrushDes.lBrushColor, NULL,
                           _T("Reg Fill Color"), PRO_Reg_Color));

    CBCGPProp* pFillProp =
        new CBCGPProp(_T("Reg Fill Style"), (_variant_t)(k_reg_fill_styles[0]),
                      _T("Reg Fill Style"), PRO_Reg_FillStyle);
    legacy_ui::add_prop_options(pFillProp, k_reg_fill_styles);
    m_wndPropList.AddProperty(pFillProp);

    CBCGPProp* pHatchProp =
        new CBCGPProp(_T("Hatch Style"), (_variant_t)(k_reg_hatch_styles[0]),
                      _T("Hatch Style"), PRO_Reg_HatchStyle);
    legacy_ui::add_prop_options(pHatchProp, k_reg_hatch_styles);
    m_wndPropList.AddProperty(pHatchProp);

    SetPropState();
  }

  return true;
}

LRESULT EditConfigDockBar::OnPropertyChanged(WPARAM, LPARAM lParam) {
  CBCGPProp* pProp = (CBCGPProp*)lParam;

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();
  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();

  switch ((int)pProp->GetData()) {
    case PRO_TEXT_Font: {
      CBCGPFontProp* pFontProp = (CBCGPFontProp*)pProp;
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
      if (pStyle) {
        SmtAnnotationDesc& stAnnoDes = pStyle->get_anno_desc();
        log_font_to_anno_desc(stAnnoDes, *(pFontProp->GetLogFont()));
        stAnnoDes.lAnnoClr = pFontProp->GetColor();
      }
    } break;
    case PRO_ChildImage_ID: {
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
      if (pStyle) {
        SmtSymbolDesc& stSymbolDes = pStyle->get_symbol_desc();
        stSymbolDes.lSymbolID = legacy_ui::prop_as_long(pProp->GetValue());
      }
    } break;
    case PRO_ChildImage_Width: {
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
      if (pStyle) {
        SmtSymbolDesc& stSymbolDes = pStyle->get_symbol_desc();
        stSymbolDes.fSymbolHeight = legacy_ui::prop_as_float(pProp->GetValue());
      }
    } break;
    case PRO_ChildImage_Height: {
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szPointStyle);
      if (pStyle) {
        SmtSymbolDesc& stSymbolDes = pStyle->get_symbol_desc();
        stSymbolDes.fSymbolWidth = legacy_ui::prop_as_float(pProp->GetValue());
      }
    } break;
    case PRO_Line_Color: {
      CBCGPColorProp* pClrProp = (CBCGPColorProp*)pProp;
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szLineStyle);
      if (pStyle) {
        SmtPenDesc& stPenDes = pStyle->get_pen_desc();
        stPenDes.lPenColor = pClrProp->GetColor();
      }
    } break;
    case PRO_Line_Style: {
      CString strStyle = (LPCTSTR)(_bstr_t)pProp->GetValue();
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szLineStyle);
      if (pStyle) {
        const int index = legacy_ui::option_index(strStyle, k_line_styles);
        if (index >= 0) {
          pStyle->get_pen_desc().lPenStyle = index;
        }
      }
    } break;
    case PRO_Line_Width: {
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szLineStyle);
      if (pStyle) {
        SmtPenDesc& stPenDes = pStyle->get_pen_desc();
        stPenDes.fPenWidth = legacy_ui::prop_as_float(pProp->GetValue());
      }
    } break;
    case PRO_Reg_Color: {
      CBCGPColorProp* pClrProp = (CBCGPColorProp*)pProp;
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szRegionStyle);
      if (pStyle) {
        SmtBrushDesc& stBrushDes = pStyle->get_brush_desc();
        stBrushDes.lBrushColor = pClrProp->GetColor();
      }
    } break;
    case PRO_Reg_FillStyle: {
      CString strStyle = (LPCTSTR)(_bstr_t)pProp->GetValue();
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szRegionStyle);
      if (pStyle) {
        const int index = legacy_ui::option_index(strStyle, k_reg_fill_styles);
        if (index == 0) {
          pStyle->get_brush_desc().brushTp = SmtBrushDesc::BT_Solid;
        } else if (index == 1) {
          pStyle->get_brush_desc().brushTp = SmtBrushDesc::BT_Hatch;
        }
        SetPropState();
      }
    } break;
    case PRO_Reg_HatchStyle: {
      CString strStyle = (LPCTSTR)(_bstr_t)pProp->GetValue();
      SmtStyle* pStyle = pStyleMgr->get_style(styleConfig.szRegionStyle);
      if (pStyle) {
        const int index = legacy_ui::option_index(strStyle, k_reg_hatch_styles);
        if (index >= 0) {
          pStyle->get_brush_desc().lBrushStyle = index;
        }
      }
    } break;
  }

  return NULL;
}

void EditConfigDockBar::SetPropState() {
  CBCGPProp* pFillTypeProp = m_wndPropList.GetProperty(PRO_Reg_FillStyle);
  CBCGPProp* pFillStyleProp = m_wndPropList.GetProperty(PRO_Reg_HatchStyle);

  CString strStyle = (LPCTSTR)(_bstr_t)pFillTypeProp->GetValue();
  if (strStyle == k_reg_fill_styles[0]) {
    pFillStyleProp->Enable(FALSE);
  } else {
    pFillStyleProp->Enable(TRUE);
  }

  if (m_wndPropList.GetSafeHwnd() != NULL) {
    m_wndPropList.RedrawWindow();
  }
}
