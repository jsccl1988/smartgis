#pragma once

#include "msvr_mapservice.h"
using namespace Smt_MapService;
// CDlgSvrCfgTile �Ի���

class CDlgSvrCfgTile : public CDialog
{
	DECLARE_DYNAMIC(CDlgSvrCfgTile)

public:
	CDlgSvrCfgTile(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgSvrCfgTile();

// �Ի�������
	enum { IDD = IDD_DLG_SVRCFG_TILE };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL		OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
	void				SetMapService(SmtMapService		*pMapService) {m_pMapService = pMapService;}

public:
	afx_msg void		OnShowWindow(BOOL bShow, UINT nStatus);

	afx_msg void		OnBnClickedBtnGenTiles();
	afx_msg void		OnBnClickedBtnViewTiles();

	afx_msg void		OnEnUpdateEditMaxLV();
	afx_msg void		OnEnUpdateEditMinLV();
	afx_msg void		OnEnUpdateEditTHeight();
	afx_msg void		OnEnUpdateEditTWidth();
	
public:
	bool				UpdateMapServiceUI(void);

protected:
	int					m_nMinLevel;
	int					m_nMaxLevel;

	int					m_nTWidth;
	int					m_nTHeight;

	CString				m_strCacheUrl;

	SmtMapService		*m_pMapService;
};
