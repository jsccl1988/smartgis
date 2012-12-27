#pragma once


// CDlgCreateMapService 对话框

class CDlgCreateMapService : public CDialog
{
	DECLARE_DYNAMIC(CDlgCreateMapService)

public:
	CDlgCreateMapService(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgCreateMapService();

// 对话框数据
	enum { IDD = IDD_DLG_CREATE_MAPSERVICE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void        OnBnClickedOk();

public:
	CString				m_strMapServiceName;
};
