
#include "stdafx.h"
#include "SysConfigDockBar.h"

#include "bl_stylemanager.h"
#include "sys_sysmanager.h"
#include "bl_api.h"

using namespace Smt_Core;
using namespace Smt_Base;
using namespace Smt_Sys;


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static char* lpszTrueOrFalse [] = 
{
	"False",
	"True",	
	NULL
};

const int nBorderSize = 1;

/////////////////////////////////////////////////////////////////////////////
// SysConfigDockBar

BEGIN_MESSAGE_MAP(SysConfigDockBar, CBCGPDockingControlBar)
	//{{AFX_MSG_MAP(SysConfigDockBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
	ON_WM_CONTEXTMENU()
	ON_REGISTERED_MESSAGE(BCGM_PROPERTY_CHANGED, OnPropertyChanged)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// SysConfigDockBar construction/destruction

SysConfigDockBar::SysConfigDockBar()
{
	// TODO: add one-time construction code here

}

SysConfigDockBar::~SysConfigDockBar()
{
	
}

/////////////////////////////////////////////////////////////////////////////
// SysConfigDockBar message handlers

int SysConfigDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty ();

	if (!m_wndPropList.Create (WS_VISIBLE | WS_CHILD, rectDummy, this, 1))
	{
		TRACE0("Failed to create Properies Grid \n");
		return -1;      // fail to create
	}

	m_wndPropList.EnableHeaderCtrl (FALSE);
	m_wndPropList.EnableDesciptionArea ();
	m_wndPropList.SetVSDotNetLook ();

	if (!CreateProList())
	{
		return -1;
	}

	return 0;
}

void SysConfigDockBar::OnSize(UINT nType, int cx, int cy) 
{
	CBCGPDockingControlBar::OnSize(nType, cx, cy);

	// Tab control should cover a whole client area:
	m_wndPropList.SetWindowPos (NULL, nBorderSize, nBorderSize, 
		cx - 2 * nBorderSize, cy - 2 * nBorderSize,
		SWP_NOACTIVATE | SWP_NOZORDER);
}

void SysConfigDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
	// TODO: 在此处添加消息处理程序代码
}

void SysConfigDockBar::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	m_wndPropList.GetWindowRect (rectTree);
	ScreenToClient (rectTree);

	rectTree.InflateRect (nBorderSize, nBorderSize);
	dc.Draw3dRect (rectTree,	::GetSysColor (COLOR_3DSHADOW), 
		::GetSysColor (COLOR_3DSHADOW));
}

bool SysConfigDockBar::CreateProList()
{
	SmtSysManager	*pSysMgr = SmtSysManager::GetSingletonPtr();
	SmtSysPra		sysPra = pSysMgr->GetSysPra();
	
	//{
	//	PRO_FLASH_CLR1,							//闪烁颜色1
	//	PRO_FLASH_CLR2,							//闪烁颜色2
	//	PRO_FLASH_ELAPSE,						//闪烁间隔
	//	PRO_2DVIEW_SMARGIN,						//屏幕容差
	//	PRO_2DVIEW_ZOOMSCALEDELT,				//视图放缩速度
	//	PRO_2DVIEW_SHOWMBR,						//显示MBR
	//	PRO_2DVIEW_SHOWPOINT,					//显示坐标点
	//	PRO_2DVIEW_POINTRADUIS,					//点半径
	//	PRO_2DVIEW_REFRESHTIME,					//2D视图刷新时间
	//	PRO_2DVIEW_NOTIFYTIME,					//2D视图系统响应时间
	//	PRO_3DVIEW_CLEARCOLOR,					//3D视图背景颜色
	//	PRO_3DVIEW_REFRESHTIME,					//3D视图刷新时间
	//	PRO_3DVIEW_NOTIFYTIME,					//3D视图系统响应时间
	//};


	m_wndPropList.AddProperty(new CBCGPColorProp(_T("Flash Color1"),sysPra.flashPra.lClr1,NULL, _T("Flash Color1"),PRO_FLASH_CLR1));
	m_wndPropList.AddProperty(new CBCGPColorProp(_T("Flash Color2"),sysPra.flashPra.lClr2,NULL, _T("Flash Color2"),PRO_FLASH_CLR2));
	m_wndPropList.AddProperty(new CBCGPProp(_T("Flash Elapse"),(_variant_t) (sysPra.flashPra.lElapse),_T("Flash Elapse"), PRO_FLASH_ELAPSE));

	m_wndPropList.AddProperty(new CBCGPProp(_T("SMargin"),(_variant_t) (sysPra.fSmargin),_T("Flash Elapse"), PRO_2DVIEW_SMARGIN));
	m_wndPropList.AddProperty(new CBCGPProp(_T("2DView Zoom Scale Delt"),(_variant_t) (sysPra.fSmargin),_T("2DView Zoom Scale Delt"), PRO_2DVIEW_ZOOMSCALEDELT));

	{
		int index = sysPra.bShowMBR?1:0;
		CBCGPProp *pProp = new CBCGPProp(_T("Show MBR"),(_variant_t) (lpszTrueOrFalse[index]),_T("Show MBR"),PRO_2DVIEW_SHOWMBR  );

		for (int i = 0; lpszTrueOrFalse [i] != NULL; i++)
			pProp->AddOption (lpszTrueOrFalse [i]);

		m_wndPropList.AddProperty(pProp);
	}

	{
		int index = sysPra.bShowPoint?1:0;
		CBCGPProp *pProp = new CBCGPProp(_T("Show Point"),(_variant_t) (lpszTrueOrFalse[index]),_T("Show Point"),PRO_2DVIEW_SHOWPOINT );

		for (int i = 0; lpszTrueOrFalse [i] != NULL; i++)
			pProp->AddOption (lpszTrueOrFalse [i]);
		
		m_wndPropList.AddProperty(pProp);
	}

	m_wndPropList.AddProperty(new CBCGPProp(_T("Point Radius"),(_variant_t) (sysPra.lPointRaduis),_T("Point Radius"), PRO_2DVIEW_POINTRADUIS));

	m_wndPropList.AddProperty(new CBCGPColorProp(_T("3D Back Color"),sysPra.l3DViewClearColor,NULL, _T("3D Back Color"),PRO_3DVIEW_CLEARCOLOR));

	SetPropState ();

	return true;
}

LRESULT SysConfigDockBar::OnPropertyChanged (WPARAM,LPARAM lParam)
{
	CBCGPProp* pProp = (CBCGPProp*) lParam;

	SmtSysManager	*pSysMgr = SmtSysManager::GetSingletonPtr();
	SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
	SmtStyleConfig  styleConfig = pSysMgr->GetSysStyleConfig();

	SmtSysPra		sysPra = pSysMgr->GetSysPra();

	BOOL bResetMDIChild = FALSE;

	//{
	//	PRO_FLASH_CLR1,							//闪烁颜色1
	//	PRO_FLASH_CLR2,							//闪烁颜色2
	//	PRO_FLASH_ELAPSE,						//闪烁间隔
	//	PRO_2DVIEW_SMARGIN,						//屏幕容差
	//	PRO_2DVIEW_ZOOMSCALEDELT,				//视图放缩速度
	//	PRO_2DVIEW_SHOWMBR,						//显示MBR
	//	PRO_2DVIEW_SHOWPOINT,					//显示坐标点
	//	PRO_2DVIEW_POINTRADUIS,					//点半径
	//	PRO_2DVIEW_REFRESHTIME,					//2D视图刷新时间
	//	PRO_2DVIEW_NOTIFYTIME,					//2D视图系统响应时间
	//	PRO_3DVIEW_CLEARCOLOR,					//3D视图背景颜色
	//	PRO_3DVIEW_REFRESHTIME,					//3D视图刷新时间
	//	PRO_3DVIEW_NOTIFYTIME,					//3D视图系统响应时间
	//};

	switch ((int) pProp->GetData ())
	{
	case PRO_FLASH_CLR1:
		{
			SmtStyle *pDotFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szDotFlashStyle1);
			SmtStyle *pLineFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szLineFlashStyle1);
			SmtStyle *pRegionFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szRegionFlashStyle1);

			SmtStyle *pDotFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szDotFlashStyle2);
			SmtStyle *pLineFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szLineFlashStyle2);
			SmtStyle *pRegionFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szRegionFlashStyle2);

			CBCGPColorProp *pClrProp = (CBCGPColorProp *)pProp;
			sysPra.flashPra.lClr1 = pClrProp->GetColor();

			pDotFlashStyle1->GetPenDesc().lPenColor = sysPra.flashPra.lClr1;
			pLineFlashStyle1->GetPenDesc().lPenColor = sysPra.flashPra.lClr1;
			pRegionFlashStyle1->GetPenDesc().lPenColor = sysPra.flashPra.lClr1;
			
			pDotFlashStyle2->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr1;
			pLineFlashStyle2->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr1;
			pRegionFlashStyle2->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr1;
		}
		break;
	case PRO_FLASH_CLR2:
		{
			SmtStyle *pDotFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szDotFlashStyle1);
			SmtStyle *pLineFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szLineFlashStyle1);
			SmtStyle *pRegionFlashStyle1 = pStyleMgr->GetStyle(styleConfig.szRegionFlashStyle1);

			SmtStyle *pDotFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szDotFlashStyle2);
			SmtStyle *pLineFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szLineFlashStyle2);
			SmtStyle *pRegionFlashStyle2 = pStyleMgr->GetStyle(styleConfig.szRegionFlashStyle2);

			CBCGPColorProp *pClrProp = (CBCGPColorProp *)pProp;
			sysPra.flashPra.lClr2 = pClrProp->GetColor();
			
			pDotFlashStyle1->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr2;
			pLineFlashStyle1->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr2;
			pRegionFlashStyle1->GetBrushDesc().lBrushColor = sysPra.flashPra.lClr2;

			pDotFlashStyle2->GetPenDesc().lPenColor = sysPra.flashPra.lClr2;
			pLineFlashStyle2->GetPenDesc().lPenColor = sysPra.flashPra.lClr2;
			pRegionFlashStyle2->GetPenDesc().lPenColor = sysPra.flashPra.lClr2;	
		}
		break;
	case PRO_FLASH_ELAPSE:
		{
			sysPra.flashPra.lElapse = (long) pProp->GetValue ();
		}
		break;
	case PRO_2DVIEW_SMARGIN:
		{
			sysPra.fSmargin = (float) pProp->GetValue ();
		}
		break;
	case PRO_2DVIEW_ZOOMSCALEDELT:
		{
			sysPra.fZoomScaleDelt = (float) pProp->GetValue ();
		}
		break;
	case PRO_2DVIEW_SHOWMBR:
		{
			CString strStyle = (LPCTSTR) (_bstr_t)pProp->GetValue ();

			for (int i = 0; lpszTrueOrFalse [i] != NULL; i++)
			{
				if (strStyle == lpszTrueOrFalse [i])
				{
					sysPra.bShowMBR = i;
					break;
				}
			}	
		}
		break;
	case PRO_2DVIEW_SHOWPOINT:
		{
			CString strStyle = (LPCTSTR) (_bstr_t)pProp->GetValue ();

			for (int i = 0; lpszTrueOrFalse [i] != NULL; i++)
			{
				if (strStyle == lpszTrueOrFalse [i])
				{
					sysPra.bShowPoint = i;
					break;
				}
			}	
		}
		break;
	case PRO_2DVIEW_POINTRADUIS:
		{
			sysPra.lPointRaduis = (long) pProp->GetValue ();
		}
		break;
	case PRO_3DVIEW_CLEARCOLOR:
		{
			sysPra.l3DViewClearColor = (long) pProp->GetValue ();
		}
		break;
	}

	pSysMgr->SetSysPra(sysPra);

	return NULL;
}

void SysConfigDockBar::SetPropState ()
{
	if (m_wndPropList.GetSafeHwnd () != NULL)
	{
		m_wndPropList.RedrawWindow ();
	}
}