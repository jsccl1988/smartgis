#pragma once


// CSmart3DView 视图
#include "vw_3dxview.h"
using namespace Smt_XView;

class CSmartGisDoc;
class CSmart3DView : public Smt3DXView
{
	DECLARE_DYNCREATE(CSmart3DView)

protected:
	CSmart3DView();           // 动态创建所使用的受保护的构造函数
	virtual ~CSmart3DView();

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

public:
	DECLARE_MESSAGE_MAP()
	afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void				OnMouseMove(UINT nFlags, CPoint point);

public:
	virtual	int					Notify(long nMsg,SmtListenerMsg &param);
};

#ifndef _DEBUG  // CSmart3DView.cpp 中的调试版本
inline CSmartGisDoc* CSmart3DView::GetDocument() const
{ return reinterpret_cast<CSmartGisDoc*>(m_pDocument); }
#endif


