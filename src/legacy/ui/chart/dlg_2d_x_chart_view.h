#pragma once

#include "legacy/ui/chart/chart.h"
#include "legacy/ui/chart/resource.h"
#include "legacy/ui/xview/view_2d.h"

using namespace ui;
using namespace ui;

// CDlg2DXChartView �Ի���

class CDlg2DXChartView : public CDialog {
  DECLARE_DYNAMIC(CDlg2DXChartView)

 public:
  CDlg2DXChartView(CWnd* pParent = NULL);  // ��׼���캯��
  virtual ~CDlg2DXChartView();

  // �Ի�������
  enum { IDD = IDD_DLG_2DXCHARTVIEW };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);  // DDX/DDV ֧��
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
