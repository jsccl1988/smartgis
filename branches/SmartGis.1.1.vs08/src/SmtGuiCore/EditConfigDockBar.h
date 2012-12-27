#ifndef _EDITCONFIGDOCKBAR_H
#define _EDITCONFIGDOCKBAR_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "smt_core.h"

class AFX_EXT_CLASS EditConfigDockBar : public CBCGPDockingControlBar
{
public:
	enum
	{
		PRO_TEXT_Font,
		PRO_ChildImage_ID,
		PRO_ChildImage_Width,
		PRO_ChildImage_Height,
		PRO_Line_Color,
		PRO_Line_Style,
		PRO_Line_Width,
		PRO_Reg_Color,
		PRO_Reg_FillStyle,
		PRO_Reg_HatchStyle,
	};

	EditConfigDockBar();
	virtual ~EditConfigDockBar();

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


#endif //_EDITCONFIGDOCKBAR_H
