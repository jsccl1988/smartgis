// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/gis/feature_info_dialog.h"

#include "legacy/ui/dialogs/detail/feature_info_grid.h"

using namespace gis;

IMPLEMENT_DYNAMIC(CDlg2DFeatureInfo, CDialog)

CDlg2DFeatureInfo::CDlg2DFeatureInfo(CWnd* pParent /*=NULL*/)
    : CDialog(CDlg2DFeatureInfo::IDD, pParent), m_pSmtFea(NULL) {}

CDlg2DFeatureInfo::~CDlg2DFeatureInfo() { m_pSmtFea = NULL; }

void CDlg2DFeatureInfo::DoDataExchange(CDataExchange* pDX) {
  DDX_Control(pDX, IDC_STEXT_GEOM, m_geomInfo);
  DDX_Control(pDX, IDC_EDIT_ATT_FILTER, m_filter_edit);
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlg2DFeatureInfo, CDialog)
ON_BN_CLICKED(IDOK, &CDlg2DFeatureInfo::OnBnClickedOk)
ON_EN_CHANGE(IDC_EDIT_ATT_FILTER, &CDlg2DFeatureInfo::OnEnChangeAttFilter)
END_MESSAGE_MAP()

bool CDlg2DFeatureInfo::create_att_property_grid() {
  CWnd* place = GetDlgItem(IDC_GRID_ATT);
  if (!place) {
    return false;
  }
  CRect rect;
  place->GetWindowRect(&rect);
  ScreenToClient(&rect);
  place->DestroyWindow();

  if (!m_attGrid.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER, rect,
                        this, IDC_GRID_ATT)) {
    return false;
  }
  m_attGrid.EnableHeaderCtrl(FALSE);
  m_attGrid.EnableDescriptionArea(TRUE);
  m_attGrid.SetVSDotNetLook(TRUE);
  m_attGrid.MarkModifiedProperties(FALSE);
  return true;
}

BOOL CDlg2DFeatureInfo::OnInitDialog() {
  CDialog::OnInitDialog();

  if (!create_att_property_grid()) {
    return FALSE;
  }

  if (m_pSmtFea) {
    update_geom_info();
    update_att_grid_content();
  }

  return TRUE;
}

void CDlg2DFeatureInfo::update_geom_info() {
  m_geomInfo.SetWindowText(ui::detail::format_feature_geom_summary(m_pSmtFea));
}

void CDlg2DFeatureInfo::update_att_grid_content() {
  ui::detail::rebuild_feature_info_grid(&m_attGrid, m_pSmtFea, m_filter_text);
}

void CDlg2DFeatureInfo::OnEnChangeAttFilter() {
  m_filter_edit.GetWindowText(m_filter_text);
  update_att_grid_content();
}

void CDlg2DFeatureInfo::OnBnClickedOk() { OnOK(); }
