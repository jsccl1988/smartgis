#ifndef _SYSCONFIGDOCKBAR_H
#define _SYSCONFIGDOCKBAR_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "smt_core.h"

class AFX_EXT_CLASS SysConfigDockBar : public CBCGPDockingControlBar
{
public:	
	enum
	{
		PRO_FLASH_CLR1,							//闪烁颜色1
		PRO_FLASH_CLR2,							//闪烁颜色2
		PRO_FLASH_ELAPSE,						//闪烁间隔
		PRO_2DVIEW_SMARGIN,						//屏幕容差
		PRO_2DVIEW_ZOOMSCALEDELT,				//视图放缩速度
		PRO_2DVIEW_SHOWMBR,						//显示MBR
		PRO_2DVIEW_SHOWPOINT,					//显示坐标点
		PRO_2DVIEW_POINTRADUIS,					//点半径
		PRO_2DVIEW_REFRESHTIME,					//2D视图刷新时间
		PRO_2DVIEW_NOTIFYTIME,					//2D视图系统响应时间
		PRO_3DVIEW_CLEARCOLOR,					//3D视图背景颜色
		PRO_3DVIEW_REFRESHTIME,					//3D视图刷新时间
		PRO_3DVIEW_NOTIFYTIME,					//3D视图系统响应时间
	};

	SysConfigDockBar();
	virtual ~SysConfigDockBar();

public:
	afx_msg int				OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void			OnSize(UINT nType, int cx, int cy);
	afx_msg void			OnPaint();

	afx_msg void			OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
	afx_msg LRESULT			OnPropertyChanged (WPARAM,LPARAM);

protected:
	bool					CreateProList();
	void					SetPropState ();

	DECLARE_MESSAGE_MAP()

protected:
	CBCGPPropList			m_wndPropList;
};


#if !defined(Export_SmtGuiCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGuiCoreD.lib")
#       else
#          pragma comment(lib,"SmtGuiCore.lib")
#	    endif  
#endif


#endif //_SYSCONFIGDOCKBAR_H
