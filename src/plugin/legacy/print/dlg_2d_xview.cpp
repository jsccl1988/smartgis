// DlgXView.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "plugin/legacy/print/map_print.h"
#include "plugin/legacy/print/dlg_2d_xview.h"

#include "legacy/ui/xcatalog/mapmgr.h"
#include "sdb/feature/feature_api.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

using namespace sdb;
using namespace sdb;
using namespace ui;

// CDlg2DXView �Ի���

IMPLEMENT_DYNAMIC(CDlg2DXView, CDialog)

CDlg2DXView::CDlg2DXView(CWnd* pParent /*=NULL*/)
	: CDialog(CDlg2DXView::IDD, pParent)
	,m_p2DXView(NULL)
{

}

CDlg2DXView::~CDlg2DXView()
{
	SMT_SAFE_DELETE(m_p2DXView);
}

void CDlg2DXView::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlg2DXView, CDialog)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BTN_SAVE, &CDlg2DXView::OnBnClickedBtnSave)
END_MESSAGE_MAP()


// CDlg2DXView ��Ϣ��������

BOOL CDlg2DXView::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ����Ӷ���ĳ�ʼ��
	if (!InitGreateXView())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlg2DXView::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: �ڴ˴�������Ϣ�����������
	//m_p2DXView->UnbindWind();
}

//////////////////////////////////////////////////////////////////////////
BOOL CDlg2DXView::InitGreateXView(void)
{
	m_p2DXView = new Smt2DXView();

	m_p2DXView->BindDlgItem(this, IDC_XVIEW_CONTAINER);

	if (m_p2DXView->GetSafeHwnd())
		m_p2DXView->OnInitialUpdate();
	else
		return FALSE;

	m_p2DXView->SetOperMap(SmtMapMgr::get_singleton_ptr()->GetSmtMapPtr());

	return TRUE;
}

void CDlg2DXView::OnBnClickedBtnSave()
{
	// TODO: �ڴ����ӿؼ�֪ͨ�����������
	if (NULL != m_p2DXView)
	{
		LPRENDERDEVICE pRenderDevice = m_p2DXView->GetRenderDevice();
		if (NULL != pRenderDevice)
		{
			static char BASED_CODE szFilter[] = "bmp Files (*.bmp)|*.bmp|\
												gif Files (*.gif)|*.gif|\
												jpg Files (*.jpg)|*.jpg|\
												ico Files (*.ico)|*.ico|\
												png Files (*.png)|*.png|\
												mng Files (*.mng)|*.mng|\
												tif Files (*.tif)|*.tif|\
												tga Files (*.tga)|*.tga|\
												pcx Files (*.pcx)|*.pcx|\
												wbmp Files (*.wbmp)|*.wbmp|\
												wmf Files (*.wmf)|*.wmf|\
												jpc Files (*.jpc)|*.jpc|\
												jp2 Files (*.jp2)|*.jp2|\
												pgx Files (*.pgx)|*.pgx|\
												pnm Files (*.pnm)|*.pnm|\
												ras Files (*.ras)|*.ras|";

			CFileDialog dlg( FALSE , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

			if( dlg.DoModal() != IDCANCEL &&
				!dlg.GetPathName().IsEmpty())
			{
				pRenderDevice->SaveImage(dlg.GetPathName());
			}
		}
	}
}