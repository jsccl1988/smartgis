/*
File:    vw_2dxview.h 

Desc:    Smt2DEditXView,Smt 2d edit view �̳���Smt2DXView

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_2DEDITXVIEW_H
#define _VW_2DEDITXVIEW_H
#if defined(XVIEW_EXPORTS)
#define XVIEW_EXPORT __declspec(dllexport)
#else
#define XVIEW_EXPORT __declspec(dllimport)
#endif


#include "ui/xview/view_2d.h"

namespace sdb {
class MapEditSession;
}

// Smt2DEditXView ��ͼ
namespace ui
{
	class XVIEW_EXPORT Smt2DEditXView : public Smt2DXView
	{
		DECLARE_DYNCREATE(Smt2DEditXView)

	protected:
		Smt2DEditXView();           // ��̬������ʹ�õ��ܱ����Ĺ��캯��
		virtual ~Smt2DEditXView();

	public:
		virtual void				OnDraw(CDC* pDC);      // ��д�Ի��Ƹ���ͼ
		afx_msg void				OnSize(UINT nType, int cx, int cy);
		afx_msg void				OnMouseMove(UINT nFlags, CPoint point);
		afx_msg void				OnTimer(UINT_PTR nIDEvent);
		afx_msg void				OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg void				OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void				OnLButtonUp(UINT nFlags, CPoint point);
		afx_msg void				OnRButtonDown(UINT nFlags, CPoint point);
		afx_msg BOOL				OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
		afx_msg void				OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void				OnRButtonDblClk(UINT nFlags, CPoint point);
		afx_msg void				OnRButtonUp(UINT nFlags, CPoint point);

		afx_msg BOOL				OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
		afx_msg BOOL				OnEraseBkgnd(CDC* pDC);
		afx_msg void				OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

#ifdef _DEBUG
		virtual void AssertValid() const;
#ifndef _WIN32_WCE
		virtual void Dump(CDumpContext& dc) const;
#endif
#endif

	protected:
		DECLARE_MESSAGE_MAP()

	public:
		void						SetOperMap(SmtMap *pSmtMap);

	protected:
		bool						InitCreate(void);
		bool						EndDestory(void);
		bool						CreateContexMenu();
		bool						CreateMainMenu();

		bool						CreateTools(void);

	protected:
		SmtBaseTool					*m_pAppendFeaTool;
		sdb::MapEditSession			*m_pMapEdits;
	};
}

#if !defined(XVIEW_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"xviewD.lib")
#       else
#          pragma comment(lib,"xview.lib")
#	    endif
#endif

#endif //_VW_2DEDITXVIEW_H