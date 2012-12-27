#pragma once


// CSmartDataSourceView 视图
#include "vw_2dxview.h"
using namespace Smt_XView;

class CSmartGisDoc;
class CSmartDataSourceView : public Smt2DXView
{
	DECLARE_DYNCREATE(CSmartDataSourceView)

protected:
	CSmartDataSourceView();								// 动态创建所使用的受保护的构造函数
	virtual ~CSmartDataSourceView();

public:
	CSmartGisDoc*				GetDocument() const;

public:
	virtual void				OnInitialUpdate();
	virtual void				OnDraw(CDC* pDC);      // 重写以绘制该视图

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

public:
	virtual	int					Notify(long nMsg,SmtListenerMsg &param);

};
#ifndef _DEBUG  // CSmart3DView.cpp 中的调试版本
inline CSmartGisDoc* CSmartDataSourceView::GetDocument() const
{ return reinterpret_cast<CSmartGisDoc*>(m_pDocument); }
#endif


