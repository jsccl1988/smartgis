#pragma once

#include "vw_2dwsxview.h"
// CDlgMapClient �Ի���
#include "msvr_mapservice.h"
#include "msvr_mapclient.h"

using namespace Smt_MapService;
using namespace Smt_MapClient;
using namespace Smt_XView;

class CDlgMapClient : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapClient)

public:
	CDlgMapClient(SmtMapService	*pMapService,CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgMapClient();

// �Ի�������
	enum { IDD = IDD_DLG_MAPCLIENT };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void		OnDestroy();
	afx_msg void		OnPaint();

	afx_msg void		OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void		OnMouseMove(UINT nFlags, CPoint point);

protected:
	BOOL				InitGreateXView(void);
	
	BOOL				InitGreateWSMap(void);

protected:
	Smt2DWSXView		*m_p2DWSXView;

	SmtMapService		*m_pMapService;
	SmtWSMap			m_wsMap;

public:
	CString				m_strTileInfo;
	fPoint				m_curPos;
};
