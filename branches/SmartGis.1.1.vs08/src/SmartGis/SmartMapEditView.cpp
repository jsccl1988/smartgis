// SmartMapEditView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmartGis.h"
#include "SmartGisDoc.h"
#include "SmartMapEditView.h"
#include "MainFrm.h"

#include "cata_mapmgr.h"
using namespace Smt_GIS;

// CSmartMapEditView

IMPLEMENT_DYNCREATE(CSmartMapEditView, Smt2DEditXView)

CSmartMapEditView::CSmartMapEditView()
{

}

CSmartMapEditView::~CSmartMapEditView()
{
}

BEGIN_MESSAGE_MAP(CSmartMapEditView, Smt2DEditXView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


// CSmartMapEditView 绘图

void CSmartMapEditView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: 在此添加绘制代码
	Smt2DEditXView::OnDraw(pDC);
}


// CSmartMapEditView 诊断

#ifdef _DEBUG
void CSmartMapEditView::AssertValid() const
{
	Smt2DEditXView::AssertValid();
}

#ifndef _WIN32_WCE
void CSmartMapEditView::Dump(CDumpContext& dc) const
{
	Smt2DEditXView::Dump(dc);
}
#endif

CSmartGisDoc* CSmartMapEditView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
	return (CSmartGisDoc*)m_pDocument;
}

#endif //_DEBUG


// CSmartMapEditView 消息处理程序
void CSmartMapEditView::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	Smt2DEditXView::OnMouseMove(nFlags, point);

	CMainFrame *pMain=  (CMainFrame *)(AfxGetApp()->m_pMainWnd);

	float x ,y ;
	if (m_pRenderDevice)
		m_pRenderDevice->DPToLP(point.x,point.y,x,y);

	CString strXY,strLB;
	strXY.Format("x=%.2f,y=%.2f",x,y);
	strLB.Format("λ=%.2f,θ=%.2f",x,y);
	pMain->SetStatusBarString(2,strXY);
	pMain->SetStatusBarString(1,strLB);
}

void CSmartMapEditView::OnInitialUpdate()
{
	Smt2DEditXView::OnInitialUpdate();

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

int CSmartMapEditView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (Smt2DEditXView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此添加您专用的创建代码

	return 0;
}

void CSmartMapEditView::OnDestroy()
{
	Smt2DEditXView::OnDestroy();

	// TODO: 在此处添加消息处理程序代码
	SmtMapMgr *pMapMgr = SmtMapMgr::GetSingletonPtr();
	pMapMgr->Unregister2DXView((void*)this);
}

void CSmartMapEditView::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	Smt2DEditXView::OnActivate(nState, pWndOther, bMinimized);

	// TODO: 在此处添加消息处理程序代码
}

void CSmartMapEditView::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	Smt2DEditXView::OnActivateApp(bActive, dwThreadID);

	// TODO: 在此处添加消息处理程序代码
}

//////////////////////////////////////////////////////////////////////////
int CSmartMapEditView::Notify(long nMsg,SmtListenerMsg &param)
{
	switch (nMsg)
	{
	case SMT_MSG_GET_SYS_2DEDITVIEW:
		*(Smt2DEditXView **)(param.lParam) = (Smt2DEditXView *)this;
		break;
	}
	return SMT_ERR_NONE;
}