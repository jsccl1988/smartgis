// DlgMapServiceMgr.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "DlgMapServiceMgr.h"

#include "smt_core.h"

// CDlgMapServiceMgr 对话框

IMPLEMENT_DYNAMIC(CDlgMapServiceMgr, CDialog)

CDlgMapServiceMgr::CDlgMapServiceMgr(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgMapServiceMgr::IDD, pParent)
	,m_pDlgSvrCfgBase(NULL)
	,m_pDlgSvrCfgTile(NULL)
{

}

CDlgMapServiceMgr::~CDlgMapServiceMgr()
{
	for (int i = 0; i < m_vWnds.size();i++)
	{
		SMT_SAFE_DELETE(m_vWnds[i]);
	}
}

void CDlgMapServiceMgr::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX,IDC_TAB_SERVICE_CFG,m_tabServiceCfg);
	DDX_Control(pDX,IDC_MAPSERVICE_TREE_CONTAINER,m_contMapService);
}


BEGIN_MESSAGE_MAP(CDlgMapServiceMgr, CDialog)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_SERVICE_CFG, &CDlgMapServiceMgr::OnTcnSelchangeTabServiceCfg)
	ON_BN_CLICKED(IDC_BTN_RUN_CONSVR, &CDlgMapServiceMgr::OnBnClickedBtnRunConSvr)
	ON_BN_CLICKED(IDC_BTN_INSTALL_SVR, &CDlgMapServiceMgr::OnBnClickedBtnInstallSvr)
	ON_BN_CLICKED(IDC_BTN_START_SVR, &CDlgMapServiceMgr::OnBnClickedBtnStartSvr)
	ON_BN_CLICKED(IDC_BTN_STOP_SVR, &CDlgMapServiceMgr::OnBnClickedBtnStopSvr)
	ON_BN_CLICKED(IDC_BTN_UNINSTALL_SVR, &CDlgMapServiceMgr::OnBnClickedBtnUninstallSvr)
	ON_BN_CLICKED(IDC_BTN_RESTART_SVR, &CDlgMapServiceMgr::OnBnClickedBtnRestartSvr)
	ON_COMMAND(SMT_MSG_SELCHANGE_MAPSERVIVCE,&CDlgMapServiceMgr::OnSelChangeMapService)
	ON_BN_CLICKED(IDOK, &CDlgMapServiceMgr::OnBnClickedOk)
END_MESSAGE_MAP()


// CDlgMapServiceMgr 消息处理程序
BOOL CDlgMapServiceMgr::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	//////////////////////////////////////////////////////////////////////////
	//1.创建服务树
	if (!CreateServiceTree())
		return FALSE;
	 
	//////////////////////////////////////////////////////////////////////////
	//1.创建服务配置页
	if (!CreateServiceCfgTab())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

//////////////////////////////////////////////////////////////////////////
bool CDlgMapServiceMgr::CreateServiceTree()
{
	//创建树
	m_cataMapService.Create(WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS , CRect(0,0,0,0),&m_contMapService,1);
	m_cataMapService.UpdateMServiceCatalogTree();
	
	//移动至合适位置
	CRect rc;
	m_contMapService.GetWindowRect(&rc);
	ScreenToClient(&rc);

	m_cataMapService.MoveWindow(&rc);
	m_cataMapService.ShowWindow(SW_SHOW);

	return true;
}

bool CDlgMapServiceMgr::CreateServiceCfgTab()
{
	//////////////////////////////////////////////////////////////////////////
	int iPage = 0;
	m_tabServiceCfg.InsertItem(iPage++,"基本信息");  
	m_tabServiceCfg.InsertItem(iPage++,"瓦片信息");  
	
	//1.
	m_pDlgSvrCfgBase = new CDlgSvrCfgBase(&m_tabServiceCfg);
	m_pDlgSvrCfgTile = new CDlgSvrCfgTile(&m_tabServiceCfg);
	
	//2.
	m_pDlgSvrCfgBase->Create(IDD_DLG_SVRCFG_BASE,&m_tabServiceCfg);
	m_pDlgSvrCfgTile->Create(IDD_DLG_SVRCFG_TILE,&m_tabServiceCfg);
	
	//3.
	m_vWnds.push_back(m_pDlgSvrCfgBase);
	m_vWnds.push_back(m_pDlgSvrCfgTile);

	//4.
	InitSetWnd();

	return true;
}

void CDlgMapServiceMgr::InitSetWnd()
{
	CRect rc;
	m_tabServiceCfg.GetClientRect(rc);
	rc.top += 20;
	rc.bottom -= 0;
	rc.left += 2;
	rc.right -= 2;

	for (int i = 0; i < m_vWnds.size();i++)
	{
		if (m_vWnds[i])
		{
			m_vWnds[i]->MoveWindow(&rc);
			m_vWnds[i]->ShowWindow(SW_SHOW);
		}
	}

	if (m_vWnds.size() > 0 && m_vWnds[0])
	{
		NMHDR nm;   
		nm.hwndFrom = m_tabServiceCfg.m_hWnd;   
		nm.code = TCN_SELCHANGE;  

		m_tabServiceCfg.SetCurSel(0);   

		SendMessage(WM_NOTIFY, IDC_TAB_SERVICE_CFG, (LPARAM)&nm);
	}
}

void CDlgMapServiceMgr::OnTcnSelchangeTabServiceCfg(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	int iPage = m_tabServiceCfg.GetCurSel();

	for (int i = 0; i < m_vWnds.size();i++)
	{
		if (m_vWnds[i])
			m_vWnds[i]->ShowWindow(SW_HIDE);
	}

	if (iPage < m_vWnds.size() && m_vWnds[iPage])
	{
		m_vWnds[iPage]->ShowWindow(SW_SHOW);
	}

	*pResult = 0;
}

void CDlgMapServiceMgr::OnBnClickedBtnRunConSvr()
{
	//system("SmtConMapSvr.exe");
	ShellExecute(NULL,"open","SmtConMapSvr.exe",NULL,NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnBnClickedBtnInstallSvr()
{
	// TODO: 在此添加控件通知处理程序代码
	//system("SmtWSMapSvr.exe -install");
	ShellExecute(NULL,"open","SmtWSMapSvr.exe","-install",NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnBnClickedBtnStartSvr()
{
	// TODO: 在此添加控件通知处理程序代码
	//system("SmtWSMapSvr.exe -start");
	ShellExecute(NULL,"open","SmtWSMapSvr.exe","-start",NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnBnClickedBtnStopSvr()
{
	// TODO: 在此添加控件通知处理程序代码
	//system("SmtWSMapSvr.exe -stop");
	ShellExecute(NULL,"open","SmtWSMapSvr.exe","-stop",NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnBnClickedBtnUninstallSvr()
{
	// TODO: 在此添加控件通知处理程序代码
	//system("SmtWSMapSvr.exe -uninstall");
	ShellExecute(NULL,"open","SmtWSMapSvr.exe","-uninstall",NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnBnClickedBtnRestartSvr()
{
	// TODO: 在此添加控件通知处理程序代码
	//system("SmtWSMapSvr.exe -restart");
	ShellExecute(NULL,"open","SmtWSMapSvr.exe","-restart",NULL,SW_SHOW);
}

void CDlgMapServiceMgr::OnSelChangeMapService(void)
{
	SmtMapServiceMgr *pMServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
	SmtMapService	*pMapService =  pMServiceMgr->GetMapService(m_cataMapService.GetSelMServiceName());
	if (NULL != pMapService &&
		NULL != m_pDlgSvrCfgBase && NULL != m_pDlgSvrCfgTile)
	{
		m_pDlgSvrCfgBase->SetMapService(pMapService);
		m_pDlgSvrCfgTile->SetMapService(pMapService);

		m_pDlgSvrCfgBase->UpdateMapServiceUI();
		m_pDlgSvrCfgTile->UpdateMapServiceUI();
	} 
}


void CDlgMapServiceMgr::OnBnClickedOk()
{
	// TODO: ÔÚ´ËÌí¼Ó¿Ø¼þÍ¨Öª´¦Àí³ÌÐò´úÂë
	UpdateData(TRUE);

	SmtMapServiceMgr *pMServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
	pMServiceMgr->SaveMSVRCfg();

	OnOK();
}