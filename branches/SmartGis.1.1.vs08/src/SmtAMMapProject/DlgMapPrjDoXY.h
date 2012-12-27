#pragma once


// CDlgMapPrjDoXY �Ի���
#include "prj_projection.h"
using namespace Smt_Prj;

class CDlgMapPrjDoXY : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrjDoXY)

public:
	CDlgMapPrjDoXY(PrjX *pPrjX,CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgMapPrjDoXY();

	// �Ի�������
	enum { IDD = IDD_DLG_PRJ_DOXY };

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

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
