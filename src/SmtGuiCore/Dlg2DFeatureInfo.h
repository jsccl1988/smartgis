#pragma once

#include "GridCtrlSupport.h"
// CDlg2DFeatureInfo 对话框
#include "resource.h"

#include "gis_feature.h"
using namespace Smt_GIS;

class CDlg2DFeatureInfo : public CDialog
{
	DECLARE_DYNAMIC(CDlg2DFeatureInfo)

public:
	CDlg2DFeatureInfo(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlg2DFeatureInfo();

// 对话框数据
	enum { IDD = IDD_DLG_2DFEATURE_INFO };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL		OnInitDialog();


	DECLARE_MESSAGE_MAP()
public:
	afx_msg void		OnBnClickedOk();

public:
	void				SetFeature(SmtFeature *pSmtFea) {m_pSmtFea = pSmtFea;}

	void				InitAttGridHead();
	void				UpdateAttGridContent();

	void				UpdateGeomInfo();

protected:
	CGridCtrl			m_attGrid;
	CStatic				m_geomInfo;

	SmtFeature			*m_pSmtFea;
};
