/*
File:    vw_2dwsxview.h 

Desc:    Smt2DWSXView,Smt 2d view 继承于SmtXView
支持地图服务展示

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _VW_2DWSXVIEW_H
#define _VW_2DWSXVIEW_H

#include "vw_xview.h"
#include "rd_renderer.h"
#include "rd_renderdevice.h"
#include "gt_grouptoolfactory.h"
#include "gis_map.h"
#include "msvr_mapclient.h"
#include "msvr_mapservice.h"

using namespace Smt_Rd;
using namespace Smt_GroupTool;
using namespace Smt_GIS;
using namespace Smt_MapClient;
using namespace Smt_MapService;

// Smt2DWSXView 视图
namespace Smt_XView
{
	class SMT_EXPORT_CLASS Smt2DWSXView : public SmtXView
	{
		DECLARE_DYNCREATE(Smt2DWSXView)

	public:
		Smt2DWSXView();           
		virtual ~Smt2DWSXView();

	public:
		LPRENDERDEVICE				GetRenderDevice(void) {return m_pRenderDevice;}

	public:
		virtual void				OnDraw(CDC* pDC);      // 重写以绘制该视图
		virtual BOOL				OnCommand(WPARAM wParam, LPARAM lParam);

#ifdef _DEBUG
		virtual void AssertValid() const;
#ifndef _WIN32_WCE
		virtual void Dump(CDumpContext& dc) const;
#endif
#endif

	protected:
		DECLARE_MESSAGE_MAP()

	public:
		afx_msg int					OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void				OnDestroy();
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

	public:
		virtual void				SetOperWSMap(SmtWSMap *pWSMap);
		SmtWSMap *					GetOperWSMap(void) {return m_pOperWSMap ;}

	protected:
		bool						InitCreate(void);
		bool						EndDestory(void);
		bool						CreateContexMenu();
		bool						CreateMainMenu(void);

		bool						CreateRender(void);
		bool						CreateTools(void);

	protected:
		UINT						m_uiNotifyTimer;
		UINT						m_uiRefreshTimer;

		LPRENDERER					m_pRenderer;
		LPRENDERDEVICE				m_pRenderDevice;

		SmtBaseTool					*m_pViewCtrlTool;
		SmtBaseTool					*m_pSelectTool;
		SmtBaseTool					*m_pFlashTool;

		SmtWSMap					*m_pOperWSMap;
	};
}

#if !defined(Export_SmtXViewCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXViewCoreD.lib")
#       else
#          pragma comment(lib,"SmtXViewCore.lib")
#	    endif
#endif

#endif //_VW_2DWSXVIEW_H