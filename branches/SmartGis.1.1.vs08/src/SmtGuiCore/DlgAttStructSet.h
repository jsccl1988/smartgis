#pragma once

#include "GridCtrlSupport.h"
#include "resource.h"
// CDlgAttStructSet 对话框

#include "gis_attribute.h"

using namespace Smt_GIS;

class CDlgAttStructSet : public CDialog
{
	DECLARE_DYNAMIC(CDlgAttStructSet)

public:
	CDlgAttStructSet(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgAttStructSet();

// 对话框数据
	enum { IDD = IDD_DLG_ATT_STRUCT_SET };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
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
