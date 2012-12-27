// Container.cpp : 实现文件
//

#include "stdafx.h"
#include "Container.h"


// CContainer

IMPLEMENT_DYNAMIC(CContainer, CStatic)

CContainer::CContainer()
{

}

CContainer::~CContainer()
{
}


BEGIN_MESSAGE_MAP(CContainer, CStatic)
END_MESSAGE_MAP()



// CContainer 消息处理程序
BOOL CContainer::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类
	CWnd *pParentWnd = GetParent();

	if (NULL != pParentWnd)
		return pParentWnd->PreTranslateMessage(pMsg);
	else
		return CStatic::PreTranslateMessage(pMsg);
}

BOOL CContainer::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: 在此添加专用代码和/或调用基类
	CWnd *pParentWnd = GetParent();

	if (NULL != pParentWnd)
		return pParentWnd->PostMessage(WM_COMMAND,wParam, lParam);
	else
		return CStatic::OnCommand(wParam, lParam);
}
