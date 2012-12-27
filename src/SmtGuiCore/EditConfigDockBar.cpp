
#include "stdafx.h"
#include "EditConfigDockBar.h"

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

static char* lpszLineStyles [] = 
{
	"SOLID",	
	"-------",
	".......",	
	"_._._._",
	"_.._.._",	
	NULL
};

static char* lpszRegFillStyles [] = 
{
	"SOLID",
	"Hatch",
	NULL
};

static char* lpszRegFillHatchStyles [] = 
{
	"-----",
	"|||||",
	"\\\\\\\\\\",
	"/////",
	"+++++",
	"xxxxx",
	NULL
};

const int nBorderSize = 1;

/////////////////////////////////////////////////////////////////////////////
// EditConfigDockBar

BEGIN_MESSAGE_MAP(EditConfigDockBar, CBCGPDockingControlBar)
	//{{AFX_MSG_MAP(EditConfigDockBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
	ON_WM_CONTEXTMENU()
	ON_REGISTERED_MESSAGE(BCGM_PROPERTY_CHANGED, OnPropertyChanged)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// EditConfigDockBar construction/destruction

EditConfigDockBar::EditConfigDockBar()
{
	// TODO: add one-time construction code here

}

EditConfigDockBar::~EditConfigDockBar()
{
	
}

/////////////////////////////////////////////////////////////////////////////
// EditConfigDockBar message handlers

int EditConfigDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

void EditConfigDockBar::OnSize(UINT nType, int cx, int cy) 
{
	CBCGPDockingControlBar::OnSize(nType, cx, cy);

	// Tab control should cover a whole client area:
	m_wndPropList.SetWindowPos (NULL, nBorderSize, nBorderSize, 
		cx - 2 * nBorderSize, cy - 2 * nBorderSize,
		SWP_NOACTIVATE | SWP_NOZORDER);
}

void EditConfigDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
	// TODO: 在此处添加消息处理程序代码
}

void EditConfigDockBar::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	m_wndPropList.GetWindowRect (rectTree);
	ScreenToClient (rectTree);

	rectTree.InflateRect (nBorderSize, nBorderSize);
	dc.Draw3dRect (rectTree,	::GetSysColor (COLOR_3DSHADOW), 
		::GetSysColor (COLOR_3DSHADOW));
}

bool EditConfigDockBar::CreateProList()
{
	SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
	SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();
	SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();


	/*
	{
	PRO_TEXT_Font,
	PRO_ChildImage_ID,
	PRO_ChildImage_Width,
	PRO_ChildImage_Height,
	PRO_Line_Color,
	PRO_Line_Style,
	PRO_Line_Width,
	PRO_Reg_Color,
	PRO_Reg_FillType,
	PRO_Reg_FillStyle,
	}
	*/

	//PRO_TEXT_Font
	SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
	if (pStyle)
	{
		LOGFONT	lgFont;
		AnnoDescToLogFont(lgFont,pStyle->GetAnnoDesc());
		m_wndPropList.AddProperty(new CBCGPFontProp(_T("Anno Font"),lgFont,CF_EFFECTS | CF_SCREENFONTS,_T("Anno Font"),PRO_TEXT_Font));
	}	

	//PRO_ChildImage
	pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
	if (pStyle)
	{
		SmtSymbolDesc stSymbolDes = pStyle->GetSymbolDesc();
		m_wndPropList.AddProperty(new CBCGPProp(_T("Symble-ID"),(_variant_t) (stSymbolDes.lSymbolID),_T("Child Image ID"), PRO_ChildImage_ID));
		m_wndPropList.AddProperty(new CBCGPProp(_T("Symble-Height"),(_variant_t) (stSymbolDes.fSymbolHeight),_T("Child Image Height"), PRO_ChildImage_Height));
		m_wndPropList.AddProperty(new CBCGPProp(_T("Symble-Width"),(_variant_t) (stSymbolDes.fSymbolWidth),_T("Child Image Width"), PRO_ChildImage_Width));
	}	

	//PRO_Line
	pStyle= pStyleMgr->GetStyle(styleSonfig.szLineStyle);
	if (pStyle)
	{
		SmtPenDesc stPenDes = pStyle->GetPenDesc();
		m_wndPropList.AddProperty(new CBCGPColorProp(_T("Line Color"),stPenDes.lPenColor,NULL, _T("Line Color"),PRO_Line_Color));

		CBCGPProp *pProp = new CBCGPProp(_T("Line Style"),(_variant_t) (lpszLineStyles[0]),_T("Line Style"), PRO_Line_Style);
		int i = 0;
		for (i = 0; lpszLineStyles [i] != NULL; i++)
		{
			pProp->AddOption (lpszLineStyles [i]);
		}

		m_wndPropList.AddProperty(pProp);

		m_wndPropList.AddProperty(new CBCGPProp(_T("Line Width"),(_variant_t) (stPenDes.fPenWidth),_T("Line Width"), PRO_Line_Width));
	}	
	
	//PRO_Reg
	pStyle= pStyleMgr->GetStyle(styleSonfig.szRegionStyle);
	if (pStyle)
	{
		SmtPenDesc stPenDes = pStyle->GetPenDesc();
		SmtBrushDesc stBrushDes = pStyle->GetBrushDesc();

		m_wndPropList.AddProperty(new CBCGPColorProp(_T("Reg Color"),stBrushDes.lBrushColor,NULL, _T("Reg Fill Color"),PRO_Reg_Color));

		CBCGPProp *pFillProp = new CBCGPProp(_T("Reg Fill Style"),(_variant_t) (lpszRegFillStyles[0]),_T("Reg Fill Style"), PRO_Reg_FillStyle);
		int i = 0;
		for (i = 0; lpszRegFillStyles[i] != NULL; i++)
		{
			pFillProp->AddOption (lpszRegFillStyles [i]);
		}

		m_wndPropList.AddProperty(pFillProp);

		CBCGPProp *pHatchProp = new CBCGPProp(_T("Hatch Style"),(_variant_t) (lpszRegFillHatchStyles[0]),_T("Hatch Style"), PRO_Reg_HatchStyle);
	
		for (i = 0; lpszRegFillHatchStyles[i] != NULL; i++)
		{
			pHatchProp->AddOption (lpszRegFillHatchStyles [i]);
		}

		m_wndPropList.AddProperty(pHatchProp);

		SetPropState ();
	}	

	return true;
}

LRESULT EditConfigDockBar::OnPropertyChanged (WPARAM,LPARAM lParam)
{
	CBCGPProp* pProp = (CBCGPProp*) lParam;

	SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
	SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();
	SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();

	BOOL bResetMDIChild = FALSE;

	switch ((int) pProp->GetData ())
	{
	case PRO_TEXT_Font:
		{
			CBCGPFontProp *pFontProp = (CBCGPFontProp *)pProp;
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
			if (pStyle)
			{
				SmtAnnotationDesc &stAnnoDes = pStyle->GetAnnoDesc();
				LogFontToAnnoDesc(stAnnoDes,*(pFontProp->GetLogFont()));
				stAnnoDes.lAnnoClr = pFontProp->GetColor();
			}	
		}
		break;
	case PRO_ChildImage_ID:
		{
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
			if (pStyle)
			{
				SmtSymbolDesc &stSymbolDes = pStyle->GetSymbolDesc();
				stSymbolDes.lSymbolID = (long) pProp->GetValue ();
			}	
		}
		break;
	case PRO_ChildImage_Width:
		{
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
			if (pStyle)
			{
				SmtSymbolDesc &stSymbolDes = pStyle->GetSymbolDesc();
				stSymbolDes.fSymbolHeight = (float) pProp->GetValue ();
			}	
		}
		break;
	case PRO_ChildImage_Height:
		{
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szPointStyle);
			if (pStyle)
			{
				SmtSymbolDesc &stSymbolDes = pStyle->GetSymbolDesc();
				stSymbolDes.fSymbolWidth = (float) pProp->GetValue ();
			}	
		}
		break;
	case PRO_Line_Color:
		{
			CBCGPColorProp *pClrProp = (CBCGPColorProp *)pProp;
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szLineStyle);
			if (pStyle)
			{
				SmtPenDesc &stPenDes = pStyle->GetPenDesc();
				stPenDes.lPenColor = pClrProp->GetColor();
			}	
		}
		break;
	case PRO_Line_Style:
		{
			CString strStyle = (LPCTSTR) (_bstr_t)pProp->GetValue ();

			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szLineStyle);
			if (pStyle)
			{
				SmtPenDesc &stPenDes = pStyle->GetPenDesc();

				for (int i = 0; lpszLineStyles [i] != NULL; i++)
				{
					if (strStyle == lpszLineStyles [i])
					{
						stPenDes.lPenStyle = i;
						break;
					}
				}
			}	
		}
		break;
	case PRO_Line_Width:
		{
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szLineStyle);
			if (pStyle)
			{
				SmtPenDesc &stPenDes = pStyle->GetPenDesc();
				stPenDes.fPenWidth = (float) pProp->GetValue ();
			}	
		}
		break;
	case PRO_Reg_Color:
		{
			CBCGPColorProp *pClrProp = (CBCGPColorProp *)pProp;
			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szRegionStyle);
			if (pStyle)
			{
				SmtBrushDesc &stBrushDes = pStyle->GetBrushDesc();
				stBrushDes.lBrushColor = pClrProp->GetColor();
			}	
		}
		break;
	case PRO_Reg_FillStyle:
		{
			CString strStyle = (LPCTSTR) (_bstr_t)pProp->GetValue ();

			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szRegionStyle);
			if (pStyle)
			{
				SmtBrushDesc &stBrushDes = pStyle->GetBrushDesc();
				
				for (int i = 0; lpszRegFillStyles [i] != NULL; i++)
				{
					if (strStyle == lpszRegFillStyles [i])
					{
						if (i == 0)
							stBrushDes.brushTp = SmtBrushDesc::BT_Solid;
						else if (i == 1)
							stBrushDes.brushTp = SmtBrushDesc::BT_Hatch;
						break;
					}
				}
				SetPropState ();
			}	
		}
		break;
	case PRO_Reg_HatchStyle:
		{
			CString strStyle = (LPCTSTR) (_bstr_t)pProp->GetValue ();

			SmtStyle *   pStyle= pStyleMgr->GetStyle(styleSonfig.szRegionStyle);
			if (pStyle)
			{
				SmtBrushDesc &stBrushDes = pStyle->GetBrushDesc();

				for (int i = 0; lpszRegFillHatchStyles [i] != NULL; i++)
				{
					if (strStyle == lpszRegFillHatchStyles [i])
					{
						stBrushDes.lBrushStyle = i;
						break;
					}
				}
			}	
		}
		break;
	}

	return NULL;
}

void EditConfigDockBar::SetPropState ()
{
	
	CBCGPProp* pFillTypeProp = m_wndPropList.GetProperty (PRO_Reg_FillStyle);
	CBCGPProp* pFillStyleProp = m_wndPropList.GetProperty (PRO_Reg_HatchStyle);

	CString strStyle = (LPCTSTR) (_bstr_t)pFillTypeProp->GetValue ();
	if (strStyle == lpszRegFillStyles [0])
	{
		pFillStyleProp->Enable(FALSE);
	}
	else
	{
		pFillStyleProp->Enable(TRUE);
	}	
	
	if (m_wndPropList.GetSafeHwnd () != NULL)
	{
		m_wndPropList.RedrawWindow ();
	}
}