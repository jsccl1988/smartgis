#include <math.h>
#include <assert.h>

#include "resource.h "
#include "gt_wsviewctrltool.h"
#include "smt_api.h"
#include "bl_api.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "t_iatoolmanager.h"
#include "sys_sysmanager.h"
#include "msvr_mapservice.h"
#include "msvr_mapclient.h"

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Base;
using namespace Smt_Sys;
using namespace Smt_MapService;
using namespace Smt_MapClient;

const string						CST_STR_MAPWSVIEWCTRL_TOOL_NAME	= "复合地图浏览";

namespace Smt_GroupTool
{
	SmtWSViewCtrlTool::SmtWSViewCtrlTool():m_bCaptured(FALSE)
		,m_usFlashed(0)
		,m_lZoomLevel(0)
		,m_viewMode(VM_ZoomOff)
	{
		SetName(CST_STR_MAPWSVIEWCTRL_TOOL_NAME.c_str());
	}

	SmtWSViewCtrlTool::~SmtWSViewCtrlTool()
	{
		UnRegisterMsg();
	}

	int SmtWSViewCtrlTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
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

		AppendFuncItems("放大",GT_MSG_WSVIEW_ZOOMIN,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("缩小",GT_MSG_WSVIEW_ZOOMOUT,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("移动",GT_MSG_WSVIEW_ZOOMMOVE,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("复位",GT_MSG_WSVIEW_ZOOMRESTORE,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("刷新",GT_MSG_WSVIEW_ZOOMREFRESH,FIM_2DVIEW|FIM_2DMFMENU);
	
		SMT_IATOOL_APPEND_MSG(GT_MSG_WSVIEW_ZOOMIN);
		SMT_IATOOL_APPEND_MSG(GT_MSG_WSVIEW_ZOOMOUT);
		SMT_IATOOL_APPEND_MSG(GT_MSG_WSVIEW_ZOOMMOVE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_WSVIEW_ZOOMRESTORE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_WSVIEW_ZOOMREFRESH);
		
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_WSVIEW_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_WSVIEW_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_ZOOMLEVEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_ZOOMLEVEL);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtWSViewCtrlTool::AuxDraw()
	{
		return SMT_ERR_NONE;
	}

	int SmtWSViewCtrlTool::Timer()
	{
		return SMT_ERR_NONE;
	}

	int SmtWSViewCtrlTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
		{
			switch (nMsg)
			{
			case GT_MSG_WSVIEW_ZOOMRESTORE:
				{
					m_viewMode = VM_ZoomRestore;
					ZoomRestore();
				}
				break;
			case GT_MSG_WSVIEW_ZOOMREFRESH:
				{
					m_viewMode = VM_ZoomRefresh;
					ZoomRefresh();
				}
				break;
			}
		}
		else
		{
			switch (nMsg)
			{
			case GT_MSG_WSVIEW_ZOOMIN:
				{
					m_viewMode = VM_ZoomIn;
				}
				break;
			case GT_MSG_WSVIEW_ZOOMOUT:
				{
					m_viewMode = VM_ZoomOut;
				}
				break;
			case GT_MSG_WSVIEW_ZOOMMOVE:
				{
					m_viewMode = VM_ZoomMove;
				}
				break;
			case GT_MSG_WSVIEW_ZOOMRESTORE:
				{
					m_viewMode = VM_ZoomRestore;
					ZoomRestore();
				}
				break;
			case GT_MSG_WSVIEW_ZOOMREFRESH:
				{
					m_viewMode = VM_ZoomRefresh;
					ZoomRefresh();
				}
				break;
			case GT_MSG_SET_WSVIEW_MODE:
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
			case GT_MSG_GET_WSVIEW_MODE:
				{
					*(ushort*)param.wParam = m_viewMode;
				}
				break;
			case GT_MSG_SET_ZOOMLEVEL:
				{
					m_lZoomLevel = *(long*)param.wParam;
				}
				break;
			case GT_MSG_GET_ZOOMLEVEL:
				{
					*(long*)param.wParam = m_lZoomLevel;
				}
				break;
			}

			SetActive();
		}
	
		return SMT_ERR_NONE;
	}

	int SmtWSViewCtrlTool::SetCursor(void)
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

	int SmtWSViewCtrlTool::LButtonDown(uint nFlags, lPoint point)
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

	int SmtWSViewCtrlTool::MouseMove(uint nFlags, lPoint point)
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

	int SmtWSViewCtrlTool::LButtonUp(uint nFlags, lPoint point)
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

	int SmtWSViewCtrlTool::RButtonDown(uint nFlags, lPoint point)
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

	int SmtWSViewCtrlTool::MouseWeel(uint nFlags, short zDelta, lPoint point) 
	{
		POINT pnt;
		pnt.x = point.x;
		pnt.y = point.y;
		ScreenToClient(m_hWnd,&pnt);

		if (zDelta < 0)
		{	
			SmtWSMap *pWSMap = dynamic_cast<SmtWSMap *>(m_pOperMap);

			if (NULL != pWSMap)
			{
				fRect	 rctVP;
				fPoint	 center;
				fPoint	 offset;
				float x1,y1,x2,y2;
				m_pRenderDevice->DPToLP(pnt.x,pnt.y,x1,y1);

				pWSMap->GetZoomCenter(center);

				pWSMap->SetZoom(pWSMap->GetZoom()-1);

				pWSMap->GetClientLBRect(rctVP);
				m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);

				m_pRenderDevice->DPToLP(pnt.x,pnt.y,x2,y2);
				
				offset.x = x2 - x1;
				offset.y = y2 - y1;

				center.x -= offset.x;
				center.y -= offset.y;

				m_pRenderDevice->ZoomMove(m_pOperMap,offset);
				pWSMap->SetZoom(pWSMap->GetZoom(),center);
				pWSMap->Sync2TileLayers();
			}
		}
		else
		{
			SmtWSMap *pWSMap = dynamic_cast<SmtWSMap *>(m_pOperMap);

			if (NULL != pWSMap)
			{
				fRect	 rctVP;
				fPoint	 center;
				fPoint	 offset;
				float x1,y1,x2,y2;

				m_pRenderDevice->DPToLP(pnt.x,pnt.y,x1,y1);

				pWSMap->GetZoomCenter(center);

				pWSMap->SetZoom(pWSMap->GetZoom()+1);
			
				pWSMap->GetClientLBRect(rctVP);
				m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);

				m_pRenderDevice->DPToLP(pnt.x,pnt.y,x2,y2);

				offset.x = x2 - x1;
				offset.y = y2 - y1;

				center.x -= offset.x;
				center.y -= offset.y;

				m_pRenderDevice->ZoomMove(m_pOperMap,offset);
				pWSMap->SetZoom(pWSMap->GetZoom(),center);
				pWSMap->Sync2TileLayers();
			}
		}

		return SMT_ERR_NONE; 
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtWSViewCtrlTool::ZoomMove(short mouse_status,Smt_Core::lPoint point)
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
					
					if (m_pOperMap != NULL)
					{
						fRect	 rctVP;
						fPoint   center;

						SmtWSMap *pWSMap = dynamic_cast<SmtWSMap *>(m_pOperMap);

						if (NULL != pWSMap)
						{
							pWSMap->GetZoomCenter(center);
							center.x -= doffset.x;
							center.y -= doffset.y;

							pWSMap->SetZoom(pWSMap->GetZoom(),center);
							pWSMap->Sync2TileLayers();

							pWSMap->GetClientLBRect(rctVP);

							m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);
						}
					}	

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

	void SmtWSViewCtrlTool::ZoomIn(short mouse_status,Smt_Core::lPoint point)
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

						if (m_pOperMap != NULL)
						{
							SmtWSMap *pWSMap = dynamic_cast<SmtWSMap *>(m_pOperMap);

							if (NULL != pWSMap)
							{
								fRect	 rctVP;
								fPoint	 center;
								fPoint	 offset;
								float x1,y1,x2,y2;

								m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);

								pWSMap->GetZoomCenter(center);
								pWSMap->SetZoom(pWSMap->GetZoom()+1);

								pWSMap->GetClientLBRect(rctVP);
								m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);

								m_pRenderDevice->DPToLP(point.x,point.y,x2,y2);

								offset.x = x2 - x1;
								offset.y = y2 - y1;

								center.x -= offset.x;
								center.y -= offset.y;

								m_pRenderDevice->ZoomMove(m_pOperMap,offset);
								pWSMap->SetZoom(pWSMap->GetZoom(),center);
								pWSMap->Sync2TileLayers();
							}
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
					}

					m_viewMode = VM_ZoomOff;
				}
				break;
			}
	}

	void SmtWSViewCtrlTool::ZoomOut(short mouse_status,Smt_Core::lPoint point)
	{
		switch (mouse_status)
		{
		case typeLButtonDown:
			break;
		case typeLButtonUp:
			{	
				if (m_pOperMap != NULL)
				{
					fRect	 rctVP;
					SmtWSMap *pWSMap = dynamic_cast<SmtWSMap *>(m_pOperMap);

					if (NULL != pWSMap)
					{
						fRect	 rctVP;
						fPoint	 center;
						fPoint	 offset;
						float x1,y1,x2,y2;

						m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);

						pWSMap->GetZoomCenter(center);
						pWSMap->SetZoom(pWSMap->GetZoom()-1);

						pWSMap->GetClientLBRect(rctVP);
						m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);

						m_pRenderDevice->DPToLP(point.x,point.y,x2,y2);

						offset.x = x2 - x1;
						offset.y = y2 - y1;

						center.x -= offset.x;
						center.y -= offset.y;

						m_pRenderDevice->ZoomMove(m_pOperMap,offset);
						pWSMap->SetZoom(pWSMap->GetZoom(),center);
						pWSMap->Sync2TileLayers();
					}
				}
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

	void SmtWSViewCtrlTool::ZoomRestore()
	{
		if (m_pOperMap != NULL)
		{
			SmtWSMap *pWSMap = (SmtWSMap *)m_pOperMap;

			fRect	 rctVP;
			fPoint	 center;
			Envelope envMap;

			pWSMap->GetEnvelope(envMap);
			center.x = (envMap.MinX + envMap.MaxX)/2.;
			center.y = (envMap.MinY + envMap.MaxY)/2.;

			pWSMap->SetZoom(pWSMap->GetZoom(),center);
			pWSMap->Sync2TileLayers();

			pWSMap->GetClientLBRect(rctVP);

			m_pRenderDevice->ZoomToRect(m_pOperMap,rctVP);
		}
	}

	void SmtWSViewCtrlTool::ZoomRefresh()
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
