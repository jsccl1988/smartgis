// Dlg2DClient.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "SmtAMMapServiceMgr.h"
#include "DlgMapClient.h"
#include "smt_api.h"
#include "bl_api.h"
#include "sde_datasourcemgr.h"


using namespace Smt_Base;
using namespace Smt_SDEDevMgr;
using namespace Smt_XView;

// CDlgMapClient �Ի���

IMPLEMENT_DYNAMIC(CDlgMapClient, CDialog)

CDlgMapClient::CDlgMapClient(SmtMapService	*pMapService,CWnd* pParent /*=NULL*/)
	: CDialog(CDlgMapClient::IDD, pParent)
	,m_p2DWSXView(NULL)
	,m_pMapService(pMapService)
	,m_strTileInfo(_T(""))
{

}

CDlgMapClient::~CDlgMapClient()
{
	m_pMapService = NULL;
}

void CDlgMapClient::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_STATIC_TITLEINFO, m_strTileInfo);
}


BEGIN_MESSAGE_MAP(CDlgMapClient, CDialog)
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_SHOWWINDOW()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


// CDlgMapClient ��Ϣ�������
BOOL CDlgMapClient::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ���Ӷ���ĳ�ʼ��
	if (!InitGreateXView())
		return FALSE;

	if (!InitGreateWSMap())
		return FALSE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CDlgMapClient::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: �ڴ˴������Ϣ����������
	//m_p2DWSXView->UnbindWind();
}

//////////////////////////////////////////////////////////////////////////
BOOL CDlgMapClient::InitGreateXView(void)
{
	m_p2DWSXView = new Smt2DWSXView();

	m_p2DWSXView->BindDlgItem(this, IDC_XVIEW_CONTAINER);

	if (m_p2DWSXView->GetSafeHwnd())
	{
		m_p2DWSXView->SetOperWSMap(&m_wsMap);
		m_p2DWSXView->OnInitialUpdate();
	}
	else
		return FALSE;

	return TRUE;
}


BOOL CDlgMapClient::InitGreateWSMap(void)
{
	SmtTileLayer *pTileLayer = SmtDataSourceMgr::CreateMemTileLayer();

	fRect    rectMap;
	Envelope envMap;
	m_pMapService->GetEnvelope(envMap);
	EnvelopeToRect(rectMap,envMap);

	pTileLayer->SetLayerName("test1");
	pTileLayer->SetLayerRect(rectMap);

	if (m_wsMap.AddTileLayer(pTileLayer,m_pMapService))
	{
		lRect    rectClient;
		fPoint	 center;
		Envelope envMap;
		m_wsMap.GetEnvelope(envMap);

		center.x = (envMap.MinX + envMap.MaxX)/2.;
		center.y = (envMap.MinY + envMap.MaxY)/2.;

		LPRENDERDEVICE pRenderDevice = m_p2DWSXView->GetRenderDevice();
		Viewport		  vp = pRenderDevice->GetViewport();
		ViewportToRect(rectClient,vp);

		m_wsMap.SetZoom(0,center);

		m_wsMap.SetClientRect(rectClient);

		return TRUE;
	}
	
	return FALSE;
}

void CDlgMapClient::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	// TODO: �ڴ˴������Ϣ����������
	// ��Ϊ��ͼ��Ϣ���� CDialog::OnPaint()
}

void CDlgMapClient::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: �ڴ˴������Ϣ����������
	if (bShow)
	{
		;
	}
}

void CDlgMapClient::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	if (NULL != m_p2DWSXView)
	{
		LPRENDERDEVICE pRenderDevice = m_p2DWSXView->GetRenderDevice();
		if (NULL != pRenderDevice)
		{
			POINT screenPoint;
			screenPoint.x = point.x;
			screenPoint.y = point.y;
			ClientToScreen(&screenPoint);

			m_p2DWSXView->ScreenToClient(&screenPoint);
			pRenderDevice->DPToLP(screenPoint.x,screenPoint.y,m_curPos.x,m_curPos.y);

			fRect	clientLBRect;
			m_wsMap.GetClientLBRect(clientLBRect);

			m_strTileInfo.Format("ZoomLevel:%d\
								  \r\nmin(%.3f,%.3f)\
								  \r\nmax(%.3f,%.3f)\
								  \r\npos(%.3f,%.3f)",\
								  m_wsMap.GetZoom(),\
								  clientLBRect.lb.x, clientLBRect.lb.y,\
								  clientLBRect.rt.x,clientLBRect.rt.y,\
								  m_curPos.x,m_curPos.y);

			UpdateData(FALSE);
		}
	}

	CDialog::OnMouseMove(nFlags, point);
}
