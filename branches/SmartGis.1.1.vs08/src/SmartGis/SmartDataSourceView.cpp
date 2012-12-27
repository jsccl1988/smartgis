// SmartDataSourceView.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "SmartGis.h"
#include "SmartGisDoc.h"
#include "SmartDataSourceView.h"
#include "cata_mapmgr.h"

using namespace Smt_GIS;
using namespace Smt_XCatalog;
// CSmartDataSourceView

IMPLEMENT_DYNCREATE(CSmartDataSourceView, Smt2DXView)

CSmartDataSourceView::CSmartDataSourceView()
{

}

CSmartDataSourceView::~CSmartDataSourceView()
{
}

BEGIN_MESSAGE_MAP(CSmartDataSourceView, Smt2DXView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CSmartDataSourceView ��ͼ

void CSmartDataSourceView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: �ڴ���ӻ��ƴ���
	Smt2DXView::OnDraw(pDC);
}


// CSmartDataSourceView ���

#ifdef _DEBUG
void CSmartDataSourceView::AssertValid() const
{
	Smt2DXView::AssertValid();
}

#ifndef _WIN32_WCE
void CSmartDataSourceView::Dump(CDumpContext& dc) const
{
	Smt2DXView::Dump(dc);
}
#endif

CSmartGisDoc* CSmartDataSourceView::GetDocument() const // �ǵ��԰汾��������
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
	return (CSmartGisDoc*)m_pDocument;
}

#endif //_DEBUG


// CSmartDataSourceView ��Ϣ�������
int CSmartDataSourceView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (Smt2DXView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  �ڴ������ר�õĴ�������

	return 0;
}

void CSmartDataSourceView::OnDestroy()
{
	Smt2DXView::OnDestroy();

	// TODO: �ڴ˴������Ϣ����������
	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	pMapMgr->Unregister2DXView((void*)this);
}


void CSmartDataSourceView::OnInitialUpdate()
{
	Smt2DXView::OnInitialUpdate();

	// TODO: �ڴ����ר�ô����/����û���
	//����mainframe �˵�
	((CSmartGisDoc*)GetDocument())->m_hCurMainMenu = m_hMainMenu;
	((CFrameWnd*)AfxGetMainWnd())->OnUpdateFrameMenu(NULL);
	AfxGetMainWnd()->DrawMenuBar();

	//���õ�ǰ��ͼ
	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	SmtMap *pSmtMap = pMapMgr->GetSmtMapPtr();
	pMapMgr->Register2DXView((void*)this);

	if (NULL != pSmtMap)
	{
		SetOperMap(pSmtMap);
	}
}

int CSmartDataSourceView::Notify(long nMsg,SmtListenerMsg &param)
{
	switch (nMsg)
	{
	case SMT_MSG_GET_SYS_2DVIEW:
		*(Smt2DXView **)(param.lParam) = (Smt2DXView *)this;
		break;
	}
	return SMT_ERR_NONE;
}

