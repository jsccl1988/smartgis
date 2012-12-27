#pragma once


// CDlgSelDS �Ի���

class CDlgSelDS : public CDialog
{
	DECLARE_DYNAMIC(CDlgSelDS)

public:
	CDlgSelDS(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgSelDS();

// �Ի�������
	enum { IDD = IDD_DLG_SEL_DS };

protected:
	virtual void			DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL			OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void			OnBnClickedOk();

	afx_msg void            OnNMClickDSTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void			OnNMDblclkDsTree(NMHDR *pNMHDR, LRESULT *pResult);

public:
	CString                 GetSelDSName(void) { return m_strSelDSName;}

private:
	void                    UpdateDSTree(void);

private:
	CString                 m_strSelDSName;
	CString					m_strSelDSType;
	CString					m_strSelDSProvider;

protected:
	CTreeCtrl               m_DSTree;
	HTREEITEM               m_hDSRoot;
};
