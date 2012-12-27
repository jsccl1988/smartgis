// DlgSvrCfgTile.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "DlgSvrCfgTile.h"
#include "msvr_mgr_api.h"
#include "smt_api.h"
#include "cata_mapservicemgr.h"
#include "bl_api.h"

using namespace Smt_Core;
using namespace Smt_XCatalog;
// CDlgSvrCfgTile 对话框

IMPLEMENT_DYNAMIC(CDlgSvrCfgTile, CDialog)

CDlgSvrCfgTile::CDlgSvrCfgTile(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSvrCfgTile::IDD, pParent)
	, m_pMapService(NULL)
	, m_nMinLevel(0)
	, m_nMaxLevel(4)
	, m_nTWidth(256)
	, m_nTHeight(256)
	, m_strCacheUrl(_T(""))
{

}

CDlgSvrCfgTile::~CDlgSvrCfgTile()
{

}

void CDlgSvrCfgTile::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MIN_LV, m_nMinLevel);
	DDX_Text(pDX, IDC_EDIT_MAX_LV, m_nMaxLevel);
	DDX_Text(pDX, IDC_EDIT_TWIDTH, m_nTWidth);
	DDX_Text(pDX, IDC_EDIT_THEIGHT, m_nTHeight);
	DDX_Text(pDX, IDC_EDIT_CACHE_URL, m_strCacheUrl);
}


BEGIN_MESSAGE_MAP(CDlgSvrCfgTile, CDialog)
	ON_BN_CLICKED(IDC_BTN_GEN_TILES, &CDlgSvrCfgTile::OnBnClickedBtnGenTiles)
	ON_BN_CLICKED(IDC_BTN_VIEWTILEMAP, &CDlgSvrCfgTile::OnBnClickedBtnViewTiles)
	ON_EN_UPDATE(IDC_EDIT_MAX_LV, &CDlgSvrCfgTile::OnEnUpdateEditMaxLV)
	ON_EN_UPDATE(IDC_EDIT_MIN_LV, &CDlgSvrCfgTile::OnEnUpdateEditMinLV)
	ON_EN_UPDATE(IDC_EDIT_THEIGHT, &CDlgSvrCfgTile::OnEnUpdateEditTHeight)
	ON_EN_UPDATE(IDC_EDIT_TWIDTH, &CDlgSvrCfgTile::OnEnUpdateEditTWidth)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CDlgSvrCfgTile 消息处理程序
BOOL CDlgSvrCfgTile::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	if (!UpdateMapServiceUI())
		return FALSE;
	 
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CDlgSvrCfgTile::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: 在此处添加消息处理程序代码
	if (bShow)
	{
		UpdateMapServiceUI();
	}
}

bool CDlgSvrCfgTile::UpdateMapServiceUI(void)
{
	if (NULL == m_pMapService)
		return true;
	 
	long lTHeight = 0,lTWidth = 0;
	m_nMinLevel = m_pMapService->GetZoomMin();
	m_nMaxLevel = m_pMapService->GetZoomMax();

	m_pMapService->GetTileSize(lTWidth,lTHeight);

	m_nTWidth	= lTWidth;
	m_nTHeight	= lTHeight;

	m_strCacheUrl = m_pMapService->GetTileCacheDir().c_str();

	UpdateData(FALSE);

	return true;
}

void CDlgSvrCfgTile::OnEnUpdateEditMaxLV()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	m_pMapService->SetZoomMax(m_nMaxLevel);
}

void CDlgSvrCfgTile::OnEnUpdateEditMinLV()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	m_pMapService->SetZoomMin(m_nMinLevel);
}

void CDlgSvrCfgTile::OnEnUpdateEditTHeight()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	m_pMapService->SetTileSize(m_nTWidth,m_nTHeight);
}

void CDlgSvrCfgTile::OnEnUpdateEditTWidth()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	m_pMapService->SetTileSize(m_nTWidth,m_nTHeight);
}

void CDlgSvrCfgTile::OnBnClickedBtnGenTiles()
{
	if (NULL == m_pMapService)
		return;

	if (m_pMapService->IsCreate())
	{
		m_pMapService->CreateTileCache();
	}
	else
	{
		::MessageBox(::GetActiveWindow(), "请先创建地图服务!", "提示", MB_OK);
	}
}

void CDlgSvrCfgTile::OnBnClickedBtnViewTiles()
{
	if (NULL == m_pMapService)
		return;

	UpdateData(TRUE);

	MSVR_MGR_ViewTileMap(m_pMapService);
}
