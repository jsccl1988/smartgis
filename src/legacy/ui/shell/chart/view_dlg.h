// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/ui/shell/chart/chart.h"
#include "legacy/ui/shell/chart/resource.h"
#include "legacy/ui/map/viewport/view_2d.h"

using namespace ui;

// Chart preview dialog hosting an Smt2DXView + SmtChart.
class CDlg2DXChartView : public CDialog {
  DECLARE_DYNAMIC(CDlg2DXChartView)

 public:
  CDlg2DXChartView(CWnd* pParent = NULL);
  virtual ~CDlg2DXChartView();

  enum { IDD = IDD_DLG_2DXCHARTVIEW };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);
  virtual BOOL OnInitDialog();

  DECLARE_MESSAGE_MAP()

 public:
  afx_msg void OnDestroy();
  afx_msg void OnBnClickedBtnSave();
  afx_msg void OnBnClickedOk();

 protected:
  BOOL InitGreateXView(void);
  BOOL InitGreateChart(void);

 protected:
  Smt2DXView* m_p2DXView;

 public:
  SmtChart m_chart;
};
