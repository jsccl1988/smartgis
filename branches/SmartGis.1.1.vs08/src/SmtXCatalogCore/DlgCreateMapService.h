#pragma once


// CDlgCreateMapService �Ի���

class CDlgCreateMapService : public CDialog
{
	DECLARE_DYNAMIC(CDlgCreateMapService)

public:
	CDlgCreateMapService(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgCreateMapService();

// �Ի�������
	enum { IDD = IDD_DLG_CREATE_MAPSERVICE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void        OnBnClickedOk();

public:
	CString				m_strMapServiceName;
};
