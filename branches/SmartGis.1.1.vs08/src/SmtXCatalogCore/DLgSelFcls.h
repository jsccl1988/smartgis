#pragma once


// CDLgSelFcls 对话框

class CDLgSelFcls : public CDialog
{
	DECLARE_DYNAMIC(CDLgSelFcls)

public:
	CDLgSelFcls(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDLgSelFcls();

	// 对话框数据
	enum { IDD = IDD_DLG_SEL_FCLS };

protected:
	virtual void            DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL            OnInitDialog();
	afx_msg void            OnBnClickedOk();
	afx_msg void            OnNMClickFclsTree(NMHDR *pNMHDR, LRESULT *pResult);

	CString                 GetSelFclsName(void) { return m_strSelFclsName;}
	UINT                    GetSelFcls(void) ;

private:
	void                    UpdateFclsTree(void);

private:
	CString                 m_strSelFclsName;

protected:
	CTreeCtrl               m_FclsTree;
	HTREEITEM               m_hFclsRoot;
};
