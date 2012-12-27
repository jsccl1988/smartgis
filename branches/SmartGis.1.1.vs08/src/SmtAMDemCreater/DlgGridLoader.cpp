// DlgGridLoader.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMDemCreater.h"
#include "DlgGridLoader.h"

#include "smt_api.h"
#include "cata_mapmgr.h"
#include "dem_gridloader.h"
#include "md3d_terrain.h"
#include "sys_sysmanager.h"
#include "gt_defs.h"
#include "dem_tinloader.h"
#include "geo_3dapi.h"
#include "cata_scenemgr.h"

using namespace Smt_GIS;
using namespace Smt_DEM;
using namespace Smt_Sys;
using namespace Smt_DEM;
using namespace Smt_XCatalog;

// CDlgGridLoader 对话框

IMPLEMENT_DYNAMIC(CDlgGridLoader, CDialog)

CDlgGridLoader::CDlgGridLoader(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgGridLoader::IDD, pParent)
	, m_p3DView(NULL)
	, m_p3DRenderDevice(NULL)
	, m_pScene(NULL)
	, m_strHMapUrl(_T(""))
	, m_fXScale(1.)
	, m_fYScale(1.)
	, m_fZScale(.1)
	, m_fXStart(0.)
	, m_fYStart(0.)
	, m_fZStart(0.)
	, m_strTexUrl(_T(""))
{
	;
}

CDlgGridLoader::~CDlgGridLoader()
{
	m_p3DView = NULL;
	m_p3DRenderDevice = NULL;
	m_pScene = NULL;
}

void CDlgGridLoader::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_HMAP_URL, m_strHMapUrl);
	
	DDX_Text(pDX, IDC_EDIT_XSCALE, m_fXScale);
	DDX_Text(pDX, IDC_EDIT_YSCALE, m_fYScale);
	DDX_Text(pDX, IDC_EDIT_ZSCALE, m_fZScale);
	DDX_Text(pDX, IDC_EDIT_XSTART, m_fXStart);
	DDX_Text(pDX, IDC_EDIT_YSTART, m_fYStart);
	DDX_Text(pDX, IDC_EDIT_ZSTART, m_fZStart);

	DDX_Text(pDX, IDC_EDIT_TEX_URL, m_strTexUrl);
	DDX_Control(pDX, IDC_CMB_CLRTYPE, m_cmbClrType);
}


BEGIN_MESSAGE_MAP(CDlgGridLoader, CDialog)
	ON_BN_CLICKED(IDC_BTN_SELHMAPFILE, &CDlgGridLoader::OnBnClickedBtnSelhmapfile)
	ON_BN_CLICKED(IDC_BTN_SELTEXFILE, &CDlgGridLoader::OnBnClickedBtnSeltexfile)
	ON_BN_CLICKED(IDOK, &CDlgGridLoader::OnBnClickedOk)
	ON_BN_CLICKED(IDC_CK_USETEX, &CDlgGridLoader::OnBnClickedCkUsetex)
	ON_EN_CHANGE(IDC_EDIT_XSCALE, &CDlgGridLoader::OnEnChangeEditXscale)
	ON_EN_CHANGE(IDC_EDIT_YSCALE, &CDlgGridLoader::OnEnChangeEditYscale)
	ON_EN_CHANGE(IDC_EDIT_ZSCALE, &CDlgGridLoader::OnEnChangeEditZscale)
	ON_EN_CHANGE(IDC_EDIT_XSTART, &CDlgGridLoader::OnEnChangeEditXstart)
	ON_EN_CHANGE(IDC_EDIT_YSTART, &CDlgGridLoader::OnEnChangeEditYstart)
	ON_EN_CHANGE(IDC_EDIT_ZSTART, &CDlgGridLoader::OnEnChangeEditZstart)
END_MESSAGE_MAP()


// CDlgGridLoader 消息处理程序

BOOL CDlgGridLoader::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	if (SMT_ERR_NONE != Init3DStuff())
		return FALSE;

	//////////////////////////////////////////////////////////////////////////
	CButton *pCkUseTex = (CButton *)GetDlgItem(IDC_CK_USETEX);
	CEdit	*pEditTexUrl = (CEdit *)GetDlgItem(IDC_EDIT_TEX_URL);
	CButton *pBtnSelTex = (CButton *)GetDlgItem(IDC_BTN_SELTEXFILE);
	if (pCkUseTex && pEditTexUrl && pBtnSelTex)
	{
		pCkUseTex->SetCheck(0);
		pEditTexUrl->EnableWindow(FALSE);
		pBtnSelTex->EnableWindow(FALSE);
		m_bUseTex = true;
	}

	//////////////////////////////////////////////////////////////////////////
	UpdateClrTypeCmb();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

int CDlgGridLoader::Init3DStuff(void)
{ 
	SmtListenerMsg msgParam;
	msgParam.lParam = (LPARAM)&m_p3DView;
	SmtPostListenerMsg(SMT_LISTENER_MSG_BROADCAST,SMT_MSG_GET_SYS_3DVIEW,msgParam);

	if (NULL == m_p3DView &&
		NULL == m_p3DView->GetSafeHwnd())
		return SMT_ERR_FAILURE;

	m_p3DRenderDevice = m_p3DView->GetRenderDevice();
	m_pScene = m_p3DView->GetScene();

	if (NULL == m_p3DRenderDevice ||
		NULL == m_pScene)
		return SMT_ERR_FAILURE;

	return SMT_ERR_NONE;
}

void CDlgGridLoader::OnBnClickedBtnSelhmapfile()
{
	// TODO: 在此添加控件通知处理程序代码
	static char BASED_CODE szFilter[] = "Data Files (*.bmp)|*.bmp|All Files (*.*)|*.*||" ;

	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "你没有选择要打开的文件!" ) ;
		return;
	}

	m_strHMapUrl = dlg.GetPathName();

	UpdateData(FALSE);
}

void CDlgGridLoader::OnBnClickedBtnSeltexfile()
{
	// TODO: 在此添加控件通知处理程序代码
	static char BASED_CODE szFilter[] = "Data Files (*.bmp)|*.bmp" ;

	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "你没有选择要打开的文件!" ) ;
		return;
	}

	char szPath[_MAX_PATH];
	char szFileName[_MAX_PATH];
	char szTitle[_MAX_PATH];
	char szExt[_MAX_PATH];

	SplitFileName(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);

	m_strTexUrl = dlg.GetPathName();
	m_strTexDir = szPath;
	m_strTexName = szTitle;
	m_strTexExt = szExt;

	UpdateData(FALSE);
}

void CDlgGridLoader::OnEnChangeEditXscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditYscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditZscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditXstart()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditYstart()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditZstart()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}


void CDlgGridLoader::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	//////////////////////////////////////////////////////////////////////////
	CString strClrType = "";
	int		nClrType = 0;
	m_cmbClrType.GetLBText(m_cmbClrType.GetCurSel(),strClrType);

	if (strClrType == "无")
		nClrType = 1;
	else if (strClrType == "等间隔")
		nClrType = 2;
	else if (strClrType == "三色阶等间距")
		nClrType = 3;

	//////////////////////////////////////////////////////////////////////////
	//加载纹理
	if(m_bUseTex && m_strTexDir != "" && m_strTexName != "")
	{
		string			strTexturePath = "";
		SmtTexture		*pTexture = NULL;

		strTexturePath = m_strTexDir + m_strTexName+m_strTexExt;
		pTexture = m_p3DRenderDevice->CreateTexture((LPCTSTR)m_strTexName);

		if (NULL != pTexture)
			pTexture->Load(strTexturePath.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	//添加地形模型
	//214、169、54
	SmtMaterial matMaterial;
	matMaterial.SetAmbientValue(SmtColor(0.2,0.2,0.2,1.0));
	matMaterial.SetDiffuseValue(SmtColor(0.8,0.8,0.8,1.0));
	matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,1.0));
	matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.0));
	matMaterial.SetShininessValue(0);

	Smt3DGridLoader			gridLoader;
	SmtTerrain *pTerrain = new SmtTerrain;

	pTerrain->SetClrType(nClrType);
	pTerrain->SetXScale(m_fXScale);
	pTerrain->SetYScale(m_fYScale);
	pTerrain->SetZScale(m_fZScale);

	gridLoader.SetXScale(m_fXScale);
	gridLoader.SetYScale(m_fYScale);
	gridLoader.SetZScale(m_fZScale);

	gridLoader.SetXStart(m_fXStart);
	gridLoader.SetYStart(m_fYStart);
	gridLoader.SetZStart(m_fZStart);

	SmtSceneMgr			*pSceneMgr = SmtSceneMgr::GetSingletonPtr();

	if (SMT_ERR_NONE == gridLoader.LoadDataFromHeightMap(m_strHMapUrl) &&
		SMT_ERR_NONE == pTerrain->Init(Vector3(30,30,30),matMaterial,m_strTexName)&& 
		SMT_ERR_NONE == pTerrain->SetTerrainSurf(gridLoader.GetGridSurf()) &&
		SMT_ERR_NONE == pTerrain->Create(m_p3DRenderDevice))
	{
		pSceneMgr->Add3DObject(pTerrain);
	}

	OnOK();
}

void CDlgGridLoader::OnBnClickedCkUsetex()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton *pCkUseTex = (CButton *)GetDlgItem(IDC_CK_USETEX);
	CEdit	*pEditTexUrl = (CEdit *)GetDlgItem(IDC_EDIT_TEX_URL);
	CButton *pBtnSelTex = (CButton *)GetDlgItem(IDC_BTN_SELTEXFILE);
	if (pCkUseTex && pEditTexUrl && pBtnSelTex)
	{
		m_bUseTex = (pCkUseTex->GetCheck() == 1);
		pEditTexUrl->EnableWindow(m_bUseTex);
		pBtnSelTex->EnableWindow(m_bUseTex);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDlgGridLoader::UpdateClrTypeCmb(void)
{
	m_cmbClrType.ResetContent();
	m_cmbClrType.AddString("无");
	m_cmbClrType.AddString("等间隔");
	m_cmbClrType.AddString("三色阶等间距");
	m_cmbClrType.SetCurSel(1);
}