// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "gis/model/feature/feature.h"
#include "legacy/ui/dialogs/resource.h"
#include "legacy/ui/widgets/bcg_cmfc.h"

using namespace gis;

// Leftover Feature Pack property grid for read-only feature attributes.
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
  // Leftover MFC shell: holds SmtFeature* for read-only display only.
  void SetFeature(SmtFeature* pSmtFea) { m_pSmtFea = pSmtFea; }

  void InitAttGridHead();
  void UpdateAttGridContent();
  void UpdateGeomInfo();

 protected:
  bool create_att_property_grid();
  void rebuild_property_groups();

  CMFCPropertyGridCtrl m_attGrid;
  CStatic m_geomInfo;
  CEdit m_filter_edit;
  CString m_filter_text;

  SmtFeature* m_pSmtFea;
};
