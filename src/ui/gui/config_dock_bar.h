#ifndef _SYSCONFIGDOCKBAR_H
#define _SYSCONFIGDOCKBAR_H
#if defined(GUI_EXPORTS)
#define GUI_EXPORT __declspec(dllexport)
#else
#define GUI_EXPORT __declspec(dllimport)
#endif


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "base/core/core.h"

class AFX_EXT_CLASS SysConfigDockBar : public CBCGPDockingControlBar
{
public:	
	enum
	{
		PRO_FLASH_CLR1,							//��˸��ɫ1
		PRO_FLASH_CLR2,							//��˸��ɫ2
		PRO_FLASH_ELAPSE,						//��˸���
		PRO_2DVIEW_SMARGIN,						//��Ļ�ݲ�
		PRO_2DVIEW_ZOOMSCALEDELT,				//��ͼ�����ٶ�
		PRO_2DVIEW_SHOWMBR,						//��ʾMBR
		PRO_2DVIEW_SHOWPOINT,					//��ʾ�����
		PRO_2DVIEW_POINTRADUIS,					//��뾶
		PRO_2DVIEW_REFRESHTIME,					//2D��ͼˢ��ʱ��
		PRO_2DVIEW_NOTIFYTIME,					//2D��ͼϵͳ��Ӧʱ��
		PRO_3DVIEW_CLEARCOLOR,					//3D��ͼ������ɫ
		PRO_3DVIEW_REFRESHTIME,					//3D��ͼˢ��ʱ��
		PRO_3DVIEW_NOTIFYTIME,					//3D��ͼϵͳ��Ӧʱ��
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


#if !defined(GUI_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"guiD.lib")
#       else
#          pragma comment(lib,"gui.lib")
#	    endif  
#endif


#endif //_SYSCONFIGDOCKBAR_H
