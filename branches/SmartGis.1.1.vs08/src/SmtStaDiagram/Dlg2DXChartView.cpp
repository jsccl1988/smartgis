// DlgXView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtStaDiagram.h"
#include "Dlg2DXChartView.h"

#include "cata_mapmgr.h"
#include "gis_api.h"
#include "sde_datasourcemgr.h"

using namespace Smt_GIS;
using namespace Smt_SDEDevMgr;
using namespace Smt_XCatalog;

// CDlg2DXChartView 对话框

IMPLEMENT_DYNAMIC(CDlg2DXChartView, CDialog)

CDlg2DXChartView::CDlg2DXChartView(CWnd* pParent /*=NULL*/)
	: CDialog(CDlg2DXChartView::IDD, pParent)
	,m_p2DXView(NULL)
{

}

CDlg2DXChartView::~CDlg2DXChartView()
{
	SMT_SAFE_DELETE(m_p2DXView);
}

void CDlg2DXChartView::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlg2DXChartView, CDialog)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BTN_SAVE, &CDlg2DXChartView::OnBnClickedBtnSave)
END_MESSAGE_MAP()


// CDlg2DXChartView 消息处理程序

BOOL CDlg2DXChartView::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	if (!InitGreateChart())
		return FALSE;

	if (!InitGreateXView())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CDlg2DXChartView::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: 在此处添加消息处理程序代码
	//m_p2DXView->UnbindWind();
}

//////////////////////////////////////////////////////////////////////////
BOOL CDlg2DXChartView::InitGreateChart(void)
{
	if (SMT_ERR_NONE == m_chart.Init() &&
		SMT_ERR_NONE == m_chart.Create())
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CDlg2DXChartView::InitGreateXView(void)
{
	m_p2DXView = new Smt2DXView();

	m_p2DXView->BindDlgItem(this, IDC_XVIEW_CONTAINER);

	if (m_p2DXView->GetSafeHwnd())
		m_p2DXView->OnInitialUpdate();
	else
		return FALSE;

	m_p2DXView->SetOperMap(m_chart.GetSmtMapPtr());

	return TRUE;
}

void CDlg2DXChartView::OnBnClickedBtnSave()
{
	// TODO: 在此添加控件通知处理程序代码
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
