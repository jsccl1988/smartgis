// dlg_grid_loader.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "plugin/dem/dem_creater.h"
#include "plugin/dem/dlg_grid_loader.h"

#include "base/core/api.h"
#include "ui/xcatalog/mapmgr.h"
#include "plugin/dem/grid_loader.h"
#include "render/terrain/terrain.h"
#include "sys/sysmanager.h"
#include "tool/group/defs.h"
#include "algorithm/geo/geometry.h"
#include "ui/xcatalog/scenemgr.h"

using namespace sdb;
using namespace plugin;
using namespace sys;
using namespace plugin;
using namespace ui;

// CDlgGridLoader �Ի���

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


// CDlgGridLoader ��Ϣ��������

BOOL CDlgGridLoader::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
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
	// �쳣: OCX ����ҳӦ���� FALSE
}

int CDlgGridLoader::Init3DStuff(void)
{ 
	SmtListenerMsg msgParam;
	msgParam.lParam = (LPARAM)&m_p3DView;
	smt_post_listener_msg(SMT_LISTENER_MSG_BROADCAST,SMT_MSG_GET_SYS_3DVIEW,msgParam);

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
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	static char BASED_CODE szFilter[] =
		"Raster (*.bmp;*.tif;*.tiff;*.img;*.asc)|*.bmp;*.tif;*.tiff;*.img;*.asc|All Files (*.*)|*.*||";

	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "��û��ѡ��Ҫ�򿪵��ļ�!" ) ;
		return;
	}

	m_strHMapUrl = dlg.GetPathName();

	UpdateData(FALSE);
}

void CDlgGridLoader::OnBnClickedBtnSeltexfile()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	static char BASED_CODE szFilter[] = "Data Files (*.bmp)|*.bmp" ;

	CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

	if( dlg.DoModal() == IDCANCEL )
	{
		AfxMessageBox( "��û��ѡ��Ҫ�򿪵��ļ�!" ) ;
		return;
	}

	char szPath[_MAX_PATH];
	char szFileName[_MAX_PATH];
	char szTitle[_MAX_PATH];
	char szExt[_MAX_PATH];

	split_file_name(dlg.GetPathName(),szPath,szFileName,szTitle,szExt);

	m_strTexUrl = dlg.GetPathName();
	m_strTexDir = szPath;
	m_strTexName = szTitle;
	m_strTexExt = szExt;

	UpdateData(FALSE);
}

void CDlgGridLoader::OnEnChangeEditXscale()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditYscale()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditZscale()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditXstart()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditYstart()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}

void CDlgGridLoader::OnEnChangeEditZstart()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ�������������
	// ���͸�֪ͨ��������д CDialog::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
}


void CDlgGridLoader::OnBnClickedOk()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	UpdateData(TRUE);
	//////////////////////////////////////////////////////////////////////////
	CString strClrType = "";
	int		nClrType = m_cmbClrType.GetCurSel() + 1;

	//////////////////////////////////////////////////////////////////////////
	//��������
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
	//���ӵ���ģ��
	//214��169��54
	SmtMaterial matMaterial;
	matMaterial.SetAmbientValue(SmtColor(0.2,0.2,0.2,1.0));
	matMaterial.SetDiffuseValue(SmtColor(0.8,0.8,0.8,1.0));
	matMaterial.SetSpecularValue(SmtColor(0.,0.,0.,1.0));
	matMaterial.SetEmissiveValue(SmtColor(0.,0.,0.,1.0));
	matMaterial.SetShininessValue(0);

	SmtTerrain *pTerrain = new SmtTerrain;

	pTerrain->SetClrType(nClrType);
	pTerrain->SetXScale(m_fXScale);
	pTerrain->SetYScale(m_fYScale);
	pTerrain->SetZScale(m_fZScale);

	GridLoadOptions grid_opt;
	grid_opt.x_scale = m_fXScale;
	grid_opt.y_scale = m_fYScale;
	grid_opt.z_scale = m_fZScale;
	grid_opt.x_start = m_fXStart;
	grid_opt.y_start = m_fYStart;
	grid_opt.z_start = m_fZStart;

	Smt3DSurface grid_surf;
	SmtSceneMgr			*pSceneMgr = SmtSceneMgr::get_singleton_ptr();
	Vector3 pos(30,30,30);

	if (SMT_ERR_NONE == load_heightmap_grid(m_strHMapUrl, grid_opt, &grid_surf) &&
		SMT_ERR_NONE == pTerrain->Init(pos,matMaterial,m_strTexName)&& 
		SMT_ERR_NONE == pTerrain->SetTerrainSurf(&grid_surf) &&
		SMT_ERR_NONE == pTerrain->Create(m_p3DRenderDevice))
	{
		pSceneMgr->Add3DObject(pTerrain);
	}

	OnOK();
}

void CDlgGridLoader::OnBnClickedCkUsetex()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
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
	m_cmbClrType.AddString("white");
	m_cmbClrType.AddString("grade");
	m_cmbClrType.AddString("color");
	m_cmbClrType.SetCurSel(1);
}