#pragma once

#include "afxcmn.h"

class CDlgMapPrjDoGrid;
class CDlgMapPrjDoXY;

class CDlgMapPrj : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrj)

public:
	CDlgMapPrj(CWnd* pParent = NULL);
	virtual ~CDlgMapPrj();

	enum { IDD = IDD_DLG_MAPPRJ };

	virtual BOOL		OnInitDialog();
protected:
	virtual void		DoDataExchange(CDataExchange* pDX);
	afx_msg void		OnTcnSelchangeTabMapprj(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()

private:
	void				sync_xy_scale();

	CTabCtrl			m_tabMapPrj;
	CDlgMapPrjDoGrid	*m_pDlgDoGrid;
	CDlgMapPrjDoXY		*m_pDlgDoXY;
};
