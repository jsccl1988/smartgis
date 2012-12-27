#pragma once


// CDlgMapPrjDoXY 对话框
#include "prj_projection.h"
using namespace Smt_Prj;

class CDlgMapPrjDoXY : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrjDoXY)

public:
	CDlgMapPrjDoXY(PrjX *pPrjX,CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgMapPrjDoXY();

	// 对话框数据
	enum { IDD = IDD_DLG_PRJ_DOXY };

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void				OnBnClickedBtnDoxy();
	afx_msg void				OnShowWindow(BOOL bShow, UINT nStatus);
	virtual BOOL				OnInitDialog();

private:
	double						m_fL;
	double						m_fB;
	double						m_fX;
	double						m_fY;

	Projection					*m_pPrj;
};
