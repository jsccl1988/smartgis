#include <math.h>
#include <assert.h>

#include "resource.h "
#include "gt_viewctrltool.h"
#include "smt_api.h"
#include "bl_api.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "t_iatoolmanager.h"
#include "sys_sysmanager.h"

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Base;
using namespace Smt_Sys;

const string						CST_STR_MAPVIEWCTRL_TOOL_NAME	= "地图浏览";

namespace Smt_GroupTool
{
	SmtViewCtrlTool::SmtViewCtrlTool():m_bCaptured(FALSE)
		,m_usFlashed(0)
		,m_viewMode(VM_ZoomOff)
	{
		SetName(CST_STR_MAPVIEWCTRL_TOOL_NAME.c_str());
	}

	SmtViewCtrlTool::~SmtViewCtrlTool()
	{
		UnRegisterMsg();
	}

	int SmtViewCtrlTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}
		
		UINT idCursors[] = 
		{
			IDC_CURSOR_ZOOMIN, 
			IDC_CURSOR_ZOOMOUT, 
			IDC_CURSOR_ZOOMMOVE,
			IDC_CURSOR_IDENTIFY
		};

		int nCount = sizeof(idCursors) / sizeof(UINT);

		for (int i = 0; i < nCount; i++)
			m_hCursors[i] = ::LoadCursor(g_hInstance, MAKEINTRESOURCE(idCursors[i]));

		AppendFuncItems("放大",GT_MSG_VIEW_ZOOMIN,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("缩小",GT_MSG_VIEW_ZOOMOUT,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("移动",GT_MSG_VIEW_ZOOMMOVE,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("复位",GT_MSG_VIEW_ZOOMRESTORE,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("刷新",GT_MSG_VIEW_ZOOMREFRESH,FIM_2DVIEW|FIM_2DMFMENU);
		//AppendFuncItems("三维视图",GT_MSG_3DVIEW_ACTIVE,FIM_2DVIEW);

		SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMIN);
		SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMOUT);
		SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMMOVE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMRESTORE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ZOOMREFRESH);
		//SMT_IATOOL_APPEND_MSG(GT_MSG_VIEW_ACTIVE);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_VIEW_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_VIEW_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SCALEDELT);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SCALEDELT);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::AuxDraw()
	{
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::Timer()
	{
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
		{
			switch (nMsg)
			{
			case GT_MSG_VIEW_ZOOMRESTORE:
				{
					m_viewMode = VM_ZoomRestore;
					ZoomRestore();
				}
				break;
			case GT_MSG_VIEW_ZOOMREFRESH:
				{
					m_viewMode = VM_ZoomRefresh;
					ZoomRefresh();
				}
				break;
			case GT_MSG_VIEW_ACTIVE:
				{
					SetForegroundWindow(m_hWnd);
				}
				break;
			}
		}
		else
		{
			switch (nMsg)
			{
			case GT_MSG_VIEW_ZOOMIN:
				{
					m_viewMode = VM_ZoomIn;
				}
				break;
			case GT_MSG_VIEW_ZOOMOUT:
				{
					m_viewMode = VM_ZoomOut;
				}
				break;
			case GT_MSG_VIEW_ZOOMMOVE:
				{
					m_viewMode = VM_ZoomMove;
				}
				break;
			case GT_MSG_VIEW_ZOOMRESTORE:
				{
					m_viewMode = VM_ZoomRestore;
					ZoomRestore();
				}
				break;
			case GT_MSG_VIEW_ZOOMREFRESH:
				{
					m_viewMode = VM_ZoomRefresh;
					ZoomRefresh();
				}
				break;
			case GT_MSG_SET_VIEW_MODE:
				{
					m_viewMode = eViewMode(*(ushort*)param.wParam);

					switch(m_viewMode)
					{
					case VM_ZoomRestore:
						ZoomRestore();
						break;
					case VM_ZoomRefresh:
						ZoomRefresh();
						break;
					default:
						break;
					}
				}
				break;
			case GT_MSG_GET_VIEW_MODE:
				{
					*(ushort*)param.wParam = m_viewMode;
				}
				break;
			case GT_MSG_SET_SCALEDELT:
				{
					m_fScaleDelt = *(double*)param.wParam;
				}
				break;
			case GT_MSG_GET_SCALEDELT:
				{
					*(double*)param.wParam = m_fScaleDelt;
				}
				break;
			}

			SetActive();
		}
	
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::SetCursor(void)
	{
		switch (m_viewMode) 
		{// Zoom mode select
		case VM_ZoomOff:
			::SetCursor(::LoadCursor(NULL,IDC_ARROW));
			break;
		case VM_ZoomIn:
			::SetCursor(m_hCursors[CursorLoupePlus]);
			break;
		case VM_ZoomOut:
			::SetCursor(m_hCursors[CursorLoupeMinus]);
			break;
		case VM_ZoomMove:
			::SetCursor(m_hCursors[CursorMove]);
			break;
		case VM_ZoomRestore:
			::SetCursor(m_hCrossCursor);
			break;
		case VM_ZoomRefresh:
			::SetCursor(m_hCrossCursor);
			break;
		default:
			// All other zoom modes
			::SetCursor(m_hCrossCursor);
			break;
		} 

		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::LButtonDown(uint nFlags, lPoint point)
	{
		SetOperDone(false);

		switch (m_viewMode)
		{
		case VM_ZoomMove:
			ZoomMove(typeLButtonDown,point);
			break;
		case VM_ZoomIn:
			ZoomIn(typeLButtonDown,point);
			break;
		case VM_ZoomOut:
			ZoomOut(typeLButtonDown,point);
			break;
		default:break;
		}
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::MouseMove(uint nFlags, lPoint point)
	{
		switch (m_viewMode)
		{
		case VM_ZoomMove:
			ZoomMove(typeMouseMove,point);
			break;
		case VM_ZoomIn:
			ZoomIn(typeMouseMove,point);
			break;
		case VM_ZoomOut:
			ZoomOut(typeMouseMove,point);
			break;
		default:break;
		}
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::LButtonUp(uint nFlags, lPoint point)
	{ 
		switch (m_viewMode)
		{
		case VM_ZoomMove:
			ZoomMove(typeLButtonUp,point);
			break;
		case VM_ZoomIn:
			ZoomIn(typeLButtonUp,point);
			break;
		case VM_ZoomOut:
			ZoomOut(typeLButtonUp,point);
			break;
		default:break;
		}
		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::RButtonDown(uint nFlags, lPoint point)
	{
		switch (m_viewMode)
		{
		case VM_ZoomMove:
			ZoomMove(typeRButtonDown,point);
			break;
		case VM_ZoomIn:
			ZoomIn(typeRButtonDown,point);
			break;
		case VM_ZoomOut:
			ZoomOut(typeRButtonDown,point);
			break;
		default:break;
		}

		SetOperDone(true);

		return SMT_ERR_NONE;
	}

	int SmtViewCtrlTool::MouseWeel(uint nFlags, short zDelta, lPoint point) 
	{
		float fScale;
		if (zDelta < 0)
		{
			fScale= 1+m_fScaleDelt;
		}
		else
			fScale= 1-m_fScaleDelt;

		POINT pnt;
		pnt.x = point.x;
		pnt.y = point.y;
		ScreenToClient(m_hWnd,&pnt);

		lPoint lpnt(pnt.x,pnt.y);
		m_pRenderDevice->ZoomScale(m_pOperMap,lpnt,fScale);
		m_pRenderDevice->Refresh();	

		return SMT_ERR_NONE; 
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtViewCtrlTool::ZoomMove(short mouse_status,Smt_Core::lPoint point)
	{
		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				m_bCaptured = TRUE;
				SetCapture(m_hWnd);
				m_pntOrigin = point;
				m_pntPrev = point;
				m_pntCur = point;

				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = (WPARAM)(&m_usFlashed);
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_GET_STATUS),param);

				if (*(ushort*)param.wParam)
				{
					SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_STOP_FLASH),param);
				}
			}
			break;
		case typeMouseMove:
			{
				m_pntPrev = m_pntCur;
				m_pntCur  = point;

				if (m_bCaptured) 
				{
					lPoint curOrgPos;
					curOrgPos.x = m_pntCur.x-m_pntOrigin.x;
					curOrgPos.y = m_pntCur.y-m_pntOrigin.y;

					m_pRenderDevice->SetCurDrawingOrg(curOrgPos);
					m_pRenderDevice->Refresh();	
				}
			}
			break;
		case typeLButtonUp:
			{
				m_pntPrev = m_pntCur;
				m_pntCur  = point;
				if (m_bCaptured) 
				{
					m_bCaptured = FALSE;
					ReleaseCapture();

					lPoint curOrgPos;
					curOrgPos.x = 0;
					curOrgPos.y = 0;

					m_pRenderDevice->SetCurDrawingOrg(curOrgPos);

					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(m_pntOrigin.x,m_pntOrigin.y,x1,y1);
					m_pRenderDevice->DPToLP(point.x,point.y,x2,y2);

					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();

					if (m_usFlashed)
					{
						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;

						SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_START_FLASH),param);
					}
				}
			}
			break;
		case typeRButtonDown:
			{
				if (m_bCaptured)
				{
					m_bCaptured = FALSE;
					ReleaseCapture();
				}

				m_viewMode = VM_ZoomOff;
			}
			break;
		default:break;
		}
	}

	void SmtViewCtrlTool::ZoomIn(short mouse_status,Smt_Core::lPoint point)
	{
			switch (mouse_status)
			{
			case typeLButtonDown:
				{	 
					m_bCaptured = TRUE;
					SetCapture(m_hWnd);
					m_pntOrigin = point;
					m_pntPrev = point;
					m_pntCur = point;
				}
				break;
			case typeLButtonUp:
				{
					if (m_bCaptured) 
					{
						m_pntPrev = m_pntCur;
						m_pntCur  = point;
						m_bCaptured = FALSE;
						ReleaseCapture();
						if (m_pntOrigin != point)
						{
							fRect frt;
							lRect lrt;
							lrt.lb.x = min(m_pntOrigin.x,m_pntCur.x);
							lrt.lb.y = max(m_pntOrigin.y,m_pntCur.y);
							lrt.rt.x = max(m_pntOrigin.x,m_pntCur.x);
							lrt.rt.y = min(m_pntOrigin.y,m_pntCur.y);

							m_pRenderDevice->DRectToLRect(lrt,frt);
						    m_pRenderDevice->ZoomToRect(m_pOperMap,frt);					
						}
						else
							m_pRenderDevice->ZoomScale(m_pOperMap,point,1-m_fScaleDelt);

						if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,true,false))
							m_pRenderDevice->EndRender(MRD_BL_QUICK);
					
						m_pRenderDevice->Refresh();			
					}
				}
				break;
			case typeMouseMove:
				{
					m_pntPrev = m_pntCur;
					m_pntCur  = point;
					if (m_bCaptured) 
					{
						SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
						SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
						if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,false,pStyle,R2_NOTXORPEN))
						{
							fRect frt1,frt2;
							frt1.Merge(m_pntOrigin.x,m_pntOrigin.y);
							frt1.Merge(m_pntPrev.x,m_pntPrev.y);
							m_pRenderDevice->DrawRect(frt1,true);

							frt2.Merge(m_pntOrigin.x,m_pntOrigin.y);
							frt2.Merge(m_pntCur.x,m_pntCur.y);
							m_pRenderDevice->DrawRect(frt2,true);

							m_pRenderDevice->EndRender(MRD_BL_QUICK);
						}
					}		
				}
				break;
			case typeRButtonDown:
				{
					m_pntCur  = point;
					if (m_bCaptured)
					{
						m_bCaptured = FALSE;
						ReleaseCapture();

						SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
						SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
						if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,false,pStyle,R2_NOTXORPEN))
						{
							fRect frt;
							frt.Merge(m_pntOrigin.x,m_pntOrigin.y);
							frt.Merge(m_pntCur.x,m_pntCur.y);
							
							m_pRenderDevice->DrawRect(frt,true);

							m_pRenderDevice->EndRender(MRD_BL_QUICK);
						}
					}

					m_viewMode = VM_ZoomOff;
				}
				break;
			}
	}

	void SmtViewCtrlTool::ZoomOut(short mouse_status,Smt_Core::lPoint point)
	{
		switch (mouse_status)
		{
		case typeLButtonDown:
			break;
		case typeLButtonUp:
			{	
				m_pRenderDevice->ZoomScale(m_pOperMap,point,1+m_fScaleDelt);
				m_pRenderDevice->Refresh();	
			}
			break;
		case typeMouseMove:
			break;
		case typeRButtonDown:
			{
				m_viewMode = VM_ZoomOff;	
			}
			break;
		}
	}

	void SmtViewCtrlTool::ZoomRestore()
	{
		if (m_pOperMap != NULL)
		{
			Envelope envelope ;
			fRect frt;

			SmtLayer *pLayer = m_pOperMap->GetActiveLayer();
			if (pLayer)
			{
				pLayer->CalEnvelope();
				pLayer->GetEnvelope(envelope);
				EnvelopeToRect(frt,envelope);

				float fWidthDiv = frt.Width()/40;
				float fHeightDiv = frt.Height()/40;

				frt.rt.x += fWidthDiv;
				frt.rt.y += fHeightDiv;
				frt.lb.x -= fWidthDiv;
				frt.lb.y -= fHeightDiv;

				m_pRenderDevice->ZoomToRect(m_pOperMap,frt);
				m_pRenderDevice->Refresh();
			}	
		}
	}

	void SmtViewCtrlTool::ZoomRefresh()
	{
		fRect frt;
		if (m_pOperMap != NULL)
		{
			Envelope envelope ;
			fRect frt;

			SmtLayer *pLayer = m_pOperMap->GetActiveLayer();
			if (pLayer)
			{
				pLayer->CalEnvelope();
				pLayer->GetEnvelope(envelope);
				EnvelopeToRect(frt,envelope);
				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}	
		}
	}
}
