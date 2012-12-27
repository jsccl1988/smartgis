#pragma once
#include "afxcmn.h"

// CDlgSelLayer 对话框

class CDlgSelLayer : public CDialog
{
	DECLARE_DYNAMIC(CDlgSelLayer)

public:
	CDlgSelLayer(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgSelLayer();

	// 对话框数据
	enum { IDD = IDD_DLG_SEL_LAYER };

protected:
	virtual void			DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL			OnInitDialog();
	afx_msg void			OnNMClickDsTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void			OnNMDblclkDsTree(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void			OnBnClickedBtnSelDs();
	afx_msg void			OnBnClickedOk();

	void                    UpdateDsTree(void);

public:
	CString                 GetSelLayerName(void) { return m_strSelLayerName;}
	CString                 GetSelDSName(void) { return m_strSelDSName;}

private:
	CString					m_strSelDSName;
	CString                 m_strSelLayerName;
	CString                 m_strSelLayerArchive;
	CString                 m_strSelLayerType;

protected:
	CTreeCtrl               m_DsTree;
	HTREEITEM               m_hDSNode;
};
