#pragma once

#include "Resource.h"
#include "vw_2dxview.h"
#include "sta_chart.h"

using namespace Smt_XView;
using namespace Smt_StaDiagram;

// CDlg2DXChartView �Ի���

class CDlg2DXChartView : public CDialog
{
	DECLARE_DYNAMIC(CDlg2DXChartView)

public:
	CDlg2DXChartView(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlg2DXChartView();

// �Ի�������
	enum { IDD = IDD_DLG_2DXCHARTVIEW };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnDestroy();
	afx_msg void		OnBnClickedBtnSave();
	afx_msg void		OnBnClickedOk();

protected:
	BOOL				InitGreateXView(void);
	BOOL				InitGreateChart(void);

protected:
	Smt2DXView			*m_p2DXView;

public:
	SmtChart			m_chart;
};
