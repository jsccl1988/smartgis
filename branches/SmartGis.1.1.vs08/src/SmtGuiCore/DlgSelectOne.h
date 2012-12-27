#pragma once


// CDlgSelectOne 对话框
#include "smt_bas_struct.h"
#include "resource.h"

class CDlgSelectOne : public CDialog
{
	DECLARE_DYNAMIC(CDlgSelectOne)

public:
	CDlgSelectOne(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgSelectOne();

// 对话框数据
	enum { IDD = IDD_DLG_SELECT_ONE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnBnClickedOk();

public:
	void				SetIDList(vector<uint> &vIDs) {m_vIDs = vIDs;}
	uint				GetSelectedID(void) const {return m_unSelID;}

	void				UpdataIDCmb(void);

protected:
	vector<uint>		m_vIDs;
	uint				m_unSelID;

	CComboBox			m_cmbIDs;
};
