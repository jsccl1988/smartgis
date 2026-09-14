#pragma once


// CDlgSelectOne �Ի���
#include "base/core/bas_struct.h"
#include "legacy/ui/gui/resource.h"

class CDlgSelectOne : public CDialog
{
	DECLARE_DYNAMIC(CDlgSelectOne)

public:
	CDlgSelectOne(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgSelectOne();

// �Ի�������
	enum { IDD = IDD_DLG_SELECT_ONE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
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
