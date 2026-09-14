#pragma once


// CSmartMapEditView ��ͼ
#include "legacy_ui/xview/view_2d_edit.h"
using namespace ui;

class CSmartGisDoc;
class CSmartMapEditView : public Smt2DEditXView
{
	DECLARE_DYNCREATE(CSmartMapEditView)

protected:
	CSmartMapEditView();           // ��̬������ʹ�õ��ܱ����Ĺ��캯��
	virtual ~CSmartMapEditView();

public:
	CSmartGisDoc*				GetDocument() const;

public:
	virtual void				OnDraw(CDC* pDC);      // ��д�Ի��Ƹ���ͼ
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

#ifndef _DEBUG  // smart_gis_view.cpp �еĵ��԰汾
inline CSmartGisDoc* CSmartMapEditView::GetDocument() const
{ return reinterpret_cast<CSmartGisDoc*>(m_pDocument); }
#endif

