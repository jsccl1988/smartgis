#pragma once

// CDlgMapPrj �Ի���
#include "prj_projection.h"
#include "afxcmn.h"

class CDlgMapPrjDoGrid;
class CDlgMapPrjDoXY;
using namespace Smt_Prj;
class CDlgMapPrj : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrj)

public:
	CDlgMapPrj(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgMapPrj();

	// �Ի�������
	enum { IDD = IDD_DLG_MAPPRJ };

	virtual BOOL		OnInitDialog();
protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	afx_msg void		OnTcnSelchangeTabMapprj(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()

private:
	CTabCtrl			m_tabMapPrj;
	CDlgMapPrjDoGrid	*m_pDlgDoGrid;
	CDlgMapPrjDoXY		*m_pDlgDoXY;

	PrjX				*m_pPrjX;
};
