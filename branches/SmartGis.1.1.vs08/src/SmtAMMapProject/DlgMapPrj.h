#pragma once

// CDlgMapPrj 对话框
#include "prj_projection.h"
#include "afxcmn.h"

class CDlgMapPrjDoGrid;
class CDlgMapPrjDoXY;
using namespace Smt_Prj;
class CDlgMapPrj : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrj)

public:
	CDlgMapPrj(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgMapPrj();

	// 对话框数据
	enum { IDD = IDD_DLG_MAPPRJ };

	virtual BOOL		OnInitDialog();
protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	afx_msg void		OnTcnSelchangeTabMapprj(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()

private:
	CTabCtrl			m_tabMapPrj;
	CDlgMapPrjDoGrid	*m_pDlgDoGrid;
	CDlgMapPrjDoXY		*m_pDlgDoXY;

	PrjX				*m_pPrjX;
};
