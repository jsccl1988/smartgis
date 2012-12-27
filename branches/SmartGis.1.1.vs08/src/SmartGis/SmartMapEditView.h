#pragma once


// CSmartMapEditView 视图
#include "vw_2deditxview.h"
using namespace Smt_XView;

class CSmartGisDoc;
class CSmartMapEditView : public Smt2DEditXView
{
	DECLARE_DYNCREATE(CSmartMapEditView)

protected:
	CSmartMapEditView();           // 动态创建所使用的受保护的构造函数
	virtual ~CSmartMapEditView();

public:
	CSmartGisDoc*				GetDocument() const;

public:
	virtual void				OnDraw(CDC* pDC);      // 重写以绘制该视图
	virtual void				OnInitialUpdate();

#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void				OnDestroy();
	afx_msg void				OnMouseMove(UINT nFlags, CPoint point);

	afx_msg void				OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void				OnActivateApp(BOOL bActive, DWORD dwThreadID);

public:
	virtual	int					Notify(long nMsg,SmtListenerMsg &param);
};

#ifndef _DEBUG  // SmartGisView.cpp 中的调试版本
inline CSmartGisDoc* CSmartMapEditView::GetDocument() const
{ return reinterpret_cast<CSmartGisDoc*>(m_pDocument); }
#endif

