// Smart3DView.cpp : 实现文件
//

#include "stdafx.h"
#include "SmartGis.h"
#include "SmartGisDoc.h"
#include "Smart3DView.h"

#include "MainFrm.h"
#include "smt_logmanager.h"
#include "cata_scenemgr.h"

using namespace Smt_Core;
using namespace Smt_XCatalog;
// CSmart3DView

IMPLEMENT_DYNCREATE(CSmart3DView, Smt3DXView)

CSmart3DView::CSmart3DView()
{

}

CSmart3DView::~CSmart3DView()
{

}

BEGIN_MESSAGE_MAP(CSmart3DView, Smt3DXView)
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


// CSmart3DView 绘图

void CSmart3DView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: 在此添加绘制代码
	Smt3DXView::OnDraw(pDC);	
}

// CSmart3DView 诊断

#ifdef _DEBUG
void CSmart3DView::AssertValid() const
{
	Smt3DXView::AssertValid();
}

#ifndef _WIN32_WCE
void CSmart3DView::Dump(CDumpContext& dc) const
{
	Smt3DXView::Dump(dc);
}
#endif


CSmartGisDoc* CSmart3DView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSmartGisDoc)));
	return (CSmartGisDoc*)m_pDocument;
}

#endif //_DEBUG

int CSmart3DView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (Smt3DXView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此添加您专用的创建代码

	return 0;
}

void CSmart3DView::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	Smt3DXView::OnMouseMove(nFlags, point);

	CMainFrame *pMain=  (CMainFrame *)(AfxGetApp()->m_pMainWnd);
	CString strXYZ;
	strXYZ.Format("x=%.4f,y=%.4f,z=%.4f",m_vCursor3DPos.x,m_vCursor3DPos.y,m_vCursor3DPos.z);
	pMain->SetStatusBarString(2,strXYZ);
}


void CSmart3DView::OnInitialUpdate()
{
	Smt3DXView::OnInitialUpdate();

	// TODO: 在此添加专用代码和/或调用基类
	((CSmartGisDoc*)GetDocument())->m_hCurMainMenu = m_hMainMenu;
	((CFrameWnd*)AfxGetMainWnd())->OnUpdateFrameMenu(NULL);
	AfxGetMainWnd()->DrawMenuBar();

	//设置当前地图
	SmtSceneMgr *pSceneMgr = SmtSceneMgr::GetSingletonPtr();
	pSceneMgr->Register3DXView((void*)this);
	pSceneMgr->AttachScene(m_pScene);
}

//////////////////////////////////////////////////////////////////////////
int CSmart3DView::Notify(long nMsg,SmtListenerMsg &param)
{
	switch (nMsg)
	{
	case SMT_MSG_GET_SYS_3DVIEW:
		*(Smt3DXView **)(param.lParam) = (Smt3DXView *)this;
		break;
	}

	return SMT_ERR_NONE;
}