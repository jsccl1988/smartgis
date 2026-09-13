#pragma once

class CDlgMapPrjDoXY : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrjDoXY)

public:
	CDlgMapPrjDoXY(CWnd* pParent = NULL);
	virtual ~CDlgMapPrjDoXY();

	enum { IDD = IDD_DLG_PRJ_DOXY };

	void					SetScaleRuler(long scale_ruler);

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void				OnBnClickedBtnDoxy();
	afx_msg void				OnShowWindow(BOOL bShow, UINT nStatus);
	virtual BOOL				OnInitDialog();

private:
	void						refresh_scale_label();

	double						m_fL;
	double						m_fB;
	double						m_fX;
	double						m_fY;
	long						m_lScaleRuler;
};
