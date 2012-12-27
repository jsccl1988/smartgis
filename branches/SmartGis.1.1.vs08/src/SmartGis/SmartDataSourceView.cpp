// SmartDataSourceView.cpp : 实现文件
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


// CSmartDataSourceView 绘图

void CSmartDataSourceView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: 在此添加绘制代码
	Smt2DXView::OnDraw(pDC);
}


// CSmartDataSourceView 诊断

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

CSmartGisDoc* CSmartDataSourceView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
	return (CSmartGisDoc*)m_pDocument;
}

#endif //_DEBUG


// CSmartDataSourceView 消息处理程序
int CSmartDataSourceView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (Smt2DXView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此添加您专用的创建代码

	return 0;
}

void CSmartDataSourceView::OnDestroy()
{
	Smt2DXView::OnDestroy();

	// TODO: 在此处添加消息处理程序代码
	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	pMapMgr->Unregister2DXView((void*)this);
}


void CSmartDataSourceView::OnInitialUpdate()
{
	Smt2DXView::OnInitialUpdate();

	// TODO: 在此添加专用代码和/或调用基类
	//重置mainframe 菜单
	((CSmartGisDoc*)GetDocument())->m_hCurMainMenu = m_hMainMenu;
	((CFrameWnd*)AfxGetMainWnd())->OnUpdateFrameMenu(NULL);
	AfxGetMainWnd()->DrawMenuBar();

	//设置当前地图
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

