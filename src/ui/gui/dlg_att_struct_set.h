#pragma once

#include "ui/mfc_ex/grid_ctrl_support.h"
#include "ui/gui/resource.h"
// CDlgAttStructSet �Ի���

#include "sdb/feature/attribute.h"

using namespace sdb;

class CDlgAttStructSet : public CDialog
{
	DECLARE_DYNAMIC(CDlgAttStructSet)

public:
	CDlgAttStructSet(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgAttStructSet();

// �Ի�������
	enum { IDD = IDD_DLG_ATT_STRUCT_SET };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void		OnBnClickedOk();
	afx_msg void		OnGridClickEndEdit(NMHDR *pNotifyStruct, LRESULT* pResult);
	afx_msg void		OnGridRClick(NMHDR *pNotifyStruct, LRESULT* pResult);

	afx_msg void		OnAttstructAppend();
	afx_msg void		OnAttstructRemove();
	afx_msg void		OnAttstructMoveup();
	afx_msg void		OnAttstructMovedown();

public:
	void				SetAttStruct(SmtAttribute *pSmtAtt,int nFixField = 0);
	void				GetAttStruct(SmtAttribute *&pSmtAtt);
	void				InitAttStructGridHead();
	void				UpdateAttStructGridContent();

protected:
	CGridCtrl			m_attStruGrid;
	CStringArray		m_arAllFldNames;
	SmtAttribute		*m_pSmtAtt;
	int					m_nFixField;

	int					m_selRow;
};
