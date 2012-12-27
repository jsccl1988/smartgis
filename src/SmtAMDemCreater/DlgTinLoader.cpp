// DlgTinLoader.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMDemCreater.h"
#include "DlgTinLoader.h"

#include "smt_api.h"
#include "cata_mapmgr.h"
#include "dem_tinloader.h"
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
using namespace Smt_XView;
using namespace Smt_XCatalog;

// CDlgTinLoader 对话框

IMPLEMENT_DYNAMIC(CDlgTinLoader, CDialog)

CDlgTinLoader::CDlgTinLoader(CWnd* pParent /*=NULL*/)
: CDialog(CDlgTinLoader::IDD, pParent)
	, m_p3DView(NULL)
	, m_p3DRenderDevice(NULL)
	, m_pScene(NULL)
	, m_strVertexUrl(_T(""))
	, m_fXScale(0.05)
	, m_fYScale(0.05)
	, m_fZScale(0.05)
	, m_iX(0)
	, m_iY(0)
	, m_iZ(0)
	, m_strTexUrl(_T(""))
	, m_nHeadSkip(1)
	, m_nLineSkip(4)
	, m_nCol(0)
	, m_nSeparator(ST_COMMA)
{

}

CDlgTinLoader::~CDlgTinLoader()
{
	m_p3DView = NULL;
	m_p3DRenderDevice = NULL;
	m_pScene = NULL;
}

void CDlgTinLoader::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GRID_COLV,m_colvGrid);
	DDX_Text(pDX, IDC_EDIT_VERTEX_URL, m_strVertexUrl);
	DDX_Control(pDX, IDC_CMB_X, m_cmbX);
	DDX_Control(pDX, IDC_CMB_Y, m_cmbY);
	DDX_Control(pDX, IDC_CMB_Z, m_cmbZ);
	DDX_Text(pDX, IDC_EDIT_XSCALE, m_fXScale);
	DDX_Text(pDX, IDC_EDIT_YSCALE, m_fYScale);
	DDX_Text(pDX, IDC_EDIT_ZSCALE, m_fZScale);
	DDX_Text(pDX, IDC_EDIT_TEX_URL, m_strTexUrl);
	DDX_Control(pDX, IDC_CMB_CLRTYPE, m_cmbClrType);
	DDX_Control(pDX, IDC_CMB_SELTINLAYER, m_cmbTinLayer);
	DDX_Text(pDX, IDC_EDIT_HEADSKIP, m_nHeadSkip);
	DDX_Text(pDX, IDC_EDIT_LINESKIP, m_nLineSkip);
}


BEGIN_MESSAGE_MAP(CDlgTinLoader, CDialog)
	ON_BN_CLICKED(IDC_BTN_SELVERTEXFILE, &CDlgTinLoader::OnBnClickedBtnSelvertexfile)
	ON_BN_CLICKED(IDC_BTN_SELTEXFILE, &CDlgTinLoader::OnBnClickedBtnSeltexfile)
	ON_BN_CLICKED(IDOK, &CDlgTinLoader::OnBnClickedOk)
	ON_BN_CLICKED(IDC_RADIO_TAB, &CDlgTinLoader::OnBnClickedRadioTab)
	ON_BN_CLICKED(IDC_RADIO_SPACE, &CDlgTinLoader::OnBnClickedRadioSpace)
	ON_BN_CLICKED(IDC_RADIO_COMMA, &CDlgTinLoader::OnBnClickedRadioComma)
	ON_BN_CLICKED(IDC_CK_USETEX, &CDlgTinLoader::OnBnClickedCkUsetex)
	ON_BN_CLICKED(IDC_CK_GEN2DTIN, &CDlgTinLoader::OnBnClickedCkGen2DTin)
	ON_EN_CHANGE(IDC_EDIT_XSCALE, &CDlgTinLoader::OnEnChangeEditXscale)
	ON_EN_CHANGE(IDC_EDIT_YSCALE, &CDlgTinLoader::OnEnChangeEditYscale)
	ON_EN_CHANGE(IDC_EDIT_ZSCALE, &CDlgTinLoader::OnEnChangeEditZscale)
END_MESSAGE_MAP()


// CDlgTinLoader 消息处理程序
BOOL CDlgTinLoader::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	//////////////////////////////////////////////////////////////////////////
	if (SMT_ERR_NONE != Init3DStuff())
		return FALSE;
	 
	//////////////////////////////////////////////////////////////////////////
	m_colvGrid.SetTextBkColor(RGB(0xFF, 0xFF, 0xE0));
	m_colvGrid.SetEditable(FALSE);

	//////////////////////////////////////////////////////////////////////////
	CButton *pRadioSepa = (CButton *)GetDlgItem(IDC_RADIO_COMMA);
	CButton *pCkUseTex = (CButton *)GetDlgItem(IDC_CK_USETEX);
	CEdit	*pEditTexUrl = (CEdit *)GetDlgItem(IDC_EDIT_TEX_URL);
	CButton *pBtnSelTex = (CButton *)GetDlgItem(IDC_BTN_SELTEXFILE);

	if (pRadioSepa)
	{
		pRadioSepa->SetCheck(1);
		m_nSeparator = ST_COMMA;
	}

	if (pCkUseTex && pEditTexUrl && pBtnSelTex)
	{
		pCkUseTex->SetCheck(0);
		pEditTexUrl->EnableWindow(FALSE);
		pBtnSelTex->EnableWindow(FALSE);
		m_bUseTex = true;
	}

	CButton *pCkGen2DTin = (CButton *)GetDlgItem(IDC_CK_GEN2DTIN);
	if (pCkGen2DTin)
	{
		pCkGen2DTin->SetCheck(0);
		m_cmbTinLayer.EnableWindow(FALSE);
		m_bGen2DTin = false;
	}
	//////////////////////////////////////////////////////////////////////////
	UpdateClrTypeCmb();

	//////////////////////////////////////////////////////////////////////////
	Update2DTinLayerCmb();

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
//////////////////////////////////////////////////////////////////////////
int CDlgTinLoader::Init3DStuff(void)
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
//////////////////////////////////////////////////////////////////////////

void CDlgTinLoader::OnBnClickedBtnSelvertexfile()
{
	// TODO: 在此添加控件通知处理程序代码
	static char BASED_CODE szFilter[] = "Data Files (*.dat)|*.dat|Txt Files (*.txt)|*.txt|All Files (*.*)|*.*||" ;

	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "你没有选择要打开的文件!" ) ;
		return;
	}

	m_strVertexUrl = dlg.GetPathName();

	OnSelVertexFile();

	UpdateData(FALSE);
}

void CDlgTinLoader::OnBnClickedBtnSeltexfile()
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

void CDlgTinLoader::OnBnClickedRadioTab()
{
	// TODO: 在此添加控件通知处理程序代码
	m_nSeparator = ST_TAB;
	CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
	CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
	CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

	if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma)
	{
		//pRadioSepaTab->SetCheck(0);
		pRadioSepaSpace->SetCheck(0);
		pRadioSepaComma->SetCheck(0);
	}

	OnChangeSeparator();
}

void CDlgTinLoader::OnBnClickedRadioSpace()
{
	// TODO: 在此添加控件通知处理程序代码
	m_nSeparator = ST_SPACE;

	CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
	CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
	CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

	if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma)
	{
		pRadioSepaTab->SetCheck(0);
		//pRadioSepaSpace->SetCheck(0);
		pRadioSepaComma->SetCheck(0);
	}

	OnChangeSeparator();
}

void CDlgTinLoader::OnBnClickedRadioComma()
{
	// TODO: 在此添加控件通知处理程序代码
	m_nSeparator = ST_COMMA;

	CButton *pRadioSepaTab = (CButton *)GetDlgItem(IDC_RADIO_TAB);
	CButton *pRadioSepaSpace = (CButton *)GetDlgItem(IDC_RADIO_SPACE);
	CButton *pRadioSepaComma = (CButton *)GetDlgItem(IDC_RADIO_COMMA);

	if (pRadioSepaTab && pRadioSepaSpace && pRadioSepaComma)
	{
		pRadioSepaTab->SetCheck(0);
		pRadioSepaSpace->SetCheck(0);
		//pRadioSepaComma->SetCheck(0);
	}
	OnChangeSeparator();
}

void CDlgTinLoader::OnChangeSeparator(void)
{
	vector<string> vFldVals;
	if (m_vTextBuf.size() > 0)
	{
		SplitString(vFldVals,m_vTextBuf[0]);

		m_nCol = vFldVals.size();

		if (m_nCol < 3)
		{
			m_cmbX.EnableWindow(FALSE);
			m_cmbY.EnableWindow(FALSE);
			m_cmbZ.EnableWindow(FALSE);
		}
		else
		{
			m_cmbX.EnableWindow(TRUE);
			m_cmbY.EnableWindow(TRUE);
			m_cmbZ.EnableWindow(TRUE);

			UpdateXYZCmb();
		}

		UpdateColVGrid();
	}
}

void CDlgTinLoader::OnBnClickedCkUsetex()
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

void CDlgTinLoader::OnBnClickedCkGen2DTin()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton *pCkGen2DTin = (CButton *)GetDlgItem(IDC_CK_GEN2DTIN);
	if (pCkGen2DTin)
	{
		m_bGen2DTin = (pCkGen2DTin->GetCheck() == 1);
		m_cmbTinLayer.EnableWindow(m_bGen2DTin);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDlgTinLoader::OnSelVertexFile(void)
{
	if (m_strVertexUrl == "")
		return;
 
	fstream fin;
	locale loc1 = locale::global(locale(".936"));
	fin.open(m_strVertexUrl,ios::in);
	locale::global(locale(loc1));

	if (!fin.is_open())
		return;

	int		nCount = 3;
	char	szBuf[2000];

	m_vTextBuf.clear();
	while(!fin.eof() && (--nCount)>0)
	{
		fin.getline(szBuf,2000,'\n');
		m_vTextBuf.push_back(szBuf);
	}

	fin.close();

	m_nCol = 1;

	OnChangeSeparator();
	UpdateColVGrid();
}

void CDlgTinLoader::UpdateColVGrid(void)
{
	//m_colvGrid.SetRedraw(FALSE);

	UpdateColVGridHead();

	UpdateColVGridContent();

	m_colvGrid.AutoSizeColumns();
	//m_colvGrid.SetRedraw(TRUE);
	m_colvGrid.Invalidate();
}

void CDlgTinLoader::UpdateColVGridHead(void)
{
	m_colvGrid.DeleteAllItems();

	m_colvGrid.SetColumnCount(m_nCol+1);
	m_colvGrid.SetRowCount(m_vTextBuf.size()+1);

	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	//序号 0
	item.row = 0;
	item.col = 0;
	item.strText = _T("序号");
	m_colvGrid.SetItem(&item);

	for (int i = 0; i < m_nCol;i++)
	{	
		item.col = i+1;
		item.strText.Format("第%d列",i+1);
		m_colvGrid.SetItem(&item);
	}
}

void CDlgTinLoader::UpdateColVGridContent(void)
{
	GV_ITEM	item;
	item.mask = GVIF_TEXT|GVIF_FORMAT;
	item.nFormat = DT_CENTER;

	vector<string> vFldVals;
	for (int i = 0; i< m_vTextBuf.size();i++)
	{
		item.row = i+1;
		item.col = 0;
		item.strText.Format("第%d行",i+1);
		m_colvGrid.SetItem(&item);

		vFldVals.clear();
		SplitString(vFldVals,m_vTextBuf[i]);

		for (int j = 0; j < vFldVals.size();j++)
		{	
			item.col = j+1;
			item.strText = vFldVals[j].c_str();
			m_colvGrid.SetItem(&item);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDlgTinLoader::UpdateClrTypeCmb(void)
{
	m_cmbClrType.ResetContent();
	m_cmbClrType.AddString("无");
	m_cmbClrType.AddString("等间隔");
	m_cmbClrType.AddString("三色阶等间距");
	m_cmbClrType.SetCurSel(1);
}

void CDlgTinLoader::UpdateXYZCmb(void)
{
	CString strColName = "";
	m_cmbX.ResetContent();
	m_cmbY.ResetContent();
	m_cmbZ.ResetContent();
	for (int i = 0; i < m_nCol;++i)
	{
		strColName.Format("第%d列",i+1);
		m_cmbX.AddString(strColName);
		m_cmbY.AddString(strColName);
		m_cmbZ.AddString(strColName);
	}

	m_cmbX.SetCurSel(0);
	m_cmbY.SetCurSel(1);
	m_cmbZ.SetCurSel(2);
}

void CDlgTinLoader::Update2DTinLayerCmb(void)
{
	SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
	m_cmbTinLayer.ResetContent();
	SmtMap *pMap = pSmtMapMgr->GetSmtMapPtr();
	for (int i = 0 ; i < pMap->GetLayerCount();i++)
	{
		SmtLayer *pLayer = pSmtMapMgr->GetLayer(i);

		if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
			continue;

		SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
		if (pVLayer && pVLayer->GetLayerFeatureType() == SmtFtTin)
		{
			m_cmbTinLayer.AddString(pLayer->GetLayerName());
		}
	}
}

void CDlgTinLoader::OnEnChangeEditXscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgTinLoader::OnEnChangeEditYscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgTinLoader::OnEnChangeEditZscale()
{
	// TODO:  如果该控件是 RICHEDIT 控件，则它将不会
	// 发送该通知，除非重写 CDialog::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	UpdateData(TRUE);
}

void CDlgTinLoader::OnBnClickedOk()
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
	//解析所选列
	CString strIX = "",strIY = "",strIZ = "";
	m_cmbX.GetLBText(m_cmbX.GetCurSel(),strIX);
	m_cmbY.GetLBText(m_cmbY.GetCurSel(),strIY);
	m_cmbZ.GetLBText(m_cmbZ.GetCurSel(),strIZ);
	sscanf(strIX,"第%d列",&m_iX);
	sscanf(strIY,"第%d列",&m_iY);
	sscanf(strIZ,"第%d列",&m_iZ);

	m_iX--;
	m_iY--;
	m_iZ--;

	if (m_iX == m_iY)
	{
		::MessageBox(::GetActiveWindow(), "XY选择不同列数据!", "提示", MB_OK);
		return;
	}
	
	//////////////////////////////////////////////////////////////////////////
	//加载纹理
	if(m_bUseTex && m_strTexDir != "" && m_strTexName != "")
	{
		string			strTexturePath = "";
		SmtTexture		*pTexture = NULL;

		//
		strTexturePath = m_strTexDir + m_strTexName+ m_strTexExt;
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

	Smt3DTinLoader			tinLoader;
	SmtTerrain *pTerrain = new SmtTerrain;

	pTerrain->SetClrType(nClrType);
	pTerrain->SetXScale(m_fXScale);
	pTerrain->SetYScale(m_fYScale);
	pTerrain->SetZScale(m_fZScale);

	tinLoader.SetXScale(m_fXScale);
	tinLoader.SetYScale(m_fYScale);
	tinLoader.SetZScale(m_fZScale);

	SmtTinFileFmt tfFmt;
	tfFmt.iX = m_iX;
	tfFmt.iY = m_iY;
	tfFmt.iZ = m_iZ;
	tfFmt.nCol = m_nCol;
	tfFmt.nHeadSkip = m_nHeadSkip;
	tfFmt.nLineSkip = m_nLineSkip;
	tfFmt.nSeparatorType = m_nSeparator;

	SmtSceneMgr			*pSceneMgr = SmtSceneMgr::GetSingletonPtr();
	
	if (SMT_ERR_NONE == tinLoader.LoadDataFromASSII(m_strVertexUrl,tfFmt) &&
		SMT_ERR_NONE == pTerrain->Init(Vector3(30,30,30),matMaterial,m_strTexName)&& 
		SMT_ERR_NONE == pTerrain->SetTerrainSurf(tinLoader.GetTinSurf()) &&
		SMT_ERR_NONE == pTerrain->Create(m_p3DRenderDevice))
	{
		pSceneMgr->Add3DObject(pTerrain);
	}

	//////////////////////////////////////////////////////////////////////////
	//创建2dtin
	if (m_bGen2DTin)
	{
		CString strTinLayer = "";
		m_cmbTinLayer.GetLBText(m_cmbTinLayer.GetCurSel(),strTinLayer);
		if (strTinLayer != "")
		{
			SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();

			SmtLayer *pLayer = pSmtMapMgr->GetLayer(strTinLayer);

			if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
				return;

			SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
		
			if (pVLayer && pVLayer->GetLayerFeatureType() == SmtFtTin)
			{
				SmtTin			oSmtTin;
				Smt3DSurface	*pTinSurf = pTerrain->GetTerrainSurf();
				if (SMT_ERR_NONE == SmtCvt3DSurfTo2DTin(pTinSurf,&oSmtTin))
				{
					SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
					SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

					SmtFeature *pSmtFeature = new SmtFeature;

					pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtTin);
					pSmtFeature->SetStyle(styleSonfig.szPointStyle);
					pSmtFeature->SetGeometry(&oSmtTin);

					if (!pSmtMapMgr->AppendFeature(pSmtFeature,false))
						SMT_SAFE_DELETE(pSmtFeature);
				}
			}
		}
	}
	
	OnOK();
}

long CDlgTinLoader::SplitString(vector<string> &vFldVal,string strContent)
{
	switch (m_nSeparator)
	{
	case ST_TAB:
		STR_Tokenize(strContent,vFldVal,"\t");
		break;
	case ST_SPACE:
		STR_Tokenize(strContent,vFldVal," ");
		break;
	case ST_COMMA:
		STR_Tokenize(strContent,vFldVal,",");
		break;
	}

	return SMT_ERR_NONE;
}