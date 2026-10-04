// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/gis/feature/model_aliases.h"
#include "legacy/ui/dialogs/resource.h"
#include "legacy/ui/widgets/feature_pack/feature_pack.h"

using namespace gis;

// GIS modal: read-only feature attributes (≈ ui/gis FeatureInfo panel).
class CDlg2DFeatureInfo : public CDialog {
  DECLARE_DYNAMIC(CDlg2DFeatureInfo)

 public:
  CDlg2DFeatureInfo(CWnd* pParent = NULL);
  ~CDlg2DFeatureInfo() override;

  enum { IDD = IDD_DLG_2DFEATURE_INFO };

 protected:
  void DoDataExchange(CDataExchange* pDX) override;
  BOOL OnInitDialog() override;

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedOk();
  afx_msg void OnEnChangeAttFilter();

 public:
  // Leftover MFC shell: holds FeatureAdapter* for read-only display only.
  void set_feature(FeatureAdapter* feature) { m_pSmtFea = feature; }

  void update_geom_info();
  void update_att_grid_content();

 protected:
  bool create_att_property_grid();

  CMFCPropertyGridCtrl m_attGrid;
  CStatic m_geomInfo;
  CEdit m_filter_edit;
  CString m_filter_text;

  FeatureAdapter* m_pSmtFea;
};
