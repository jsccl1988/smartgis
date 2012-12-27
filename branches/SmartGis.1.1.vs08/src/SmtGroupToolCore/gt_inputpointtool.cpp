#include <math.h>

#include "smt_api.h"
#include "gt_inputpointtool.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "geo_geometry.h"
#include "gt_defs.h"
#include "sys_sysmanager.h"
#include "bl_api.h"

#include "resource.h "

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Geo;
using namespace Smt_Sys;

const string						CST_STR_INPUTPOINT_TOOL_NAME	= "ÊäÈëµã";

namespace Smt_GroupTool
{
	SmtInputPointTool::SmtInputPointTool():m_pGeom(NULL)
		,m_bIsDrag(false)
		,m_bDelay(false)
		,m_appendType(-1)
		,m_fAngle(0.)
	{
		 SetName(CST_STR_INPUTPOINT_TOOL_NAME.c_str());
	}

	SmtInputPointTool::~SmtInputPointTool()
	{
		SMT_SAFE_DELETE(m_pGeom);

		UnRegisterMsg();
	}

	int SmtInputPointTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
		pStyle->SetStyleType(ST_PenDesc|ST_BrushDesc|ST_SymbolDesc|ST_AnnoDesc);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_INPUT_POINT_TYPE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_INPUT_POINT_TYPE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::AuxDraw()
	{
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{

			}
			break;
		case GT_MSG_SET_INPUT_POINT_TYPE:
			{
				m_appendType = *(ushort*)param.wParam;
			}
			break;
		case GT_MSG_GET_INPUT_ANNO_ANGLE:
			{
				*(float*)param.wParam = m_fAngle;
			}
			break;
		case GT_MSG_GET_INPUT_POINT_TYPE:
			{
				*(ushort*)param.wParam = m_appendType;
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::LButtonDown(uint nFlags, lPoint point)
	{
		SetOperDone(false);

		POINT pnt;
		GetCursorPos(&pnt);
		ScreenToClient(m_hWnd,&pnt);

		lPoint lpnt(pnt.x,pnt.y);

		switch(m_appendType)
		{
		case PT_ChildImage:
			AppendChildImage(typeLButtonDown,point);
			break;
		case PT_Text:
			AppendText(typeLButtonDown,point);
			break;
		case PT_DOT:
			AppendDot(typeLButtonDown,point);
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::MouseMove(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case PT_ChildImage:
			AppendChildImage(typeMouseMove,point);
			break;
		case PT_Text:
			AppendText(typeMouseMove,point);
			break;
		case PT_DOT:
			AppendDot(typeMouseMove,point);
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::LButtonUp(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case PT_ChildImage:
			AppendChildImage(typeLButtonUp,point);
			break;
		case PT_Text:
			AppendText(typeLButtonUp,point);
		    break;
		case PT_DOT:
			AppendDot(typeLButtonUp,point);
			break;
		default:
		    break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::RButtonDown(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case PT_ChildImage:
			AppendChildImage(typeRButtonDown,point);
			break;
		case PT_Text:
			AppendText(typeRButtonDown,point);
			break;
		case PT_DOT:
			AppendDot(typeRButtonDown,point);
			break;
		default:
			break;
		}

		if (IsOperDone())
		{
			if (m_bDelay)
				m_bDelay = false;
			else
				SetEnableContexMenu(true);		
		}
		return SMT_ERR_NONE;
	}

	int SmtInputPointTool::MouseWeel(uint nFlags, short zDelta, lPoint point) 
	{	
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtInputPointTool::AppendChildImage(short mouse_status,lPoint point)
	{
		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				m_pntOrigin = point;
				m_pntPrev = point;
				m_pntCur = point;
				 
				float x,y;
				m_pRenderDevice->DPToLP(m_pntOrigin.x,m_pntOrigin.y,x,y);
				m_pGeom = new SmtPoint(x,y);

				SetOperDone(false);
				SetEnableContexMenu(false);
				EndAppendPoint();
			}	
			break;
		}
	}

	void SmtInputPointTool::AppendText(short mouse_status,lPoint point)
	{
		static fPoint fpts[2];
		
		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				m_pntOrigin = point;
				m_pntPrev = point;
				m_pntCur = point;
				m_bIsDrag = 1;

				SetOperDone(false);
				SetEnableContexMenu(false);
			}
			
			break;
		case typeLButtonUp:
			if(m_bIsDrag)
			{			
				m_bIsDrag = 0;
				m_pntCur = point;

				SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
				SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
				if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,false,pStyle,R2_NOTXORPEN))
				{
					fpts[0].x = m_pntOrigin.x;
					fpts[0].y = m_pntOrigin.y;
					fpts[1].x = m_pntCur.x;
					fpts[1].y = m_pntCur.y;
				
					m_pRenderDevice->DrawLine(fpts,2,true);

					m_pRenderDevice->EndRender(MRD_BL_QUICK);
				}
			
				float x,y;
				m_fAngle = 0;
				m_pRenderDevice->DPToLP(m_pntOrigin.x,m_pntOrigin.y,x,y);
				m_pGeom = new SmtPoint(x,y);
				if(m_pntOrigin != m_pntCur) 
				{ 
					m_fAngle = atanf(float(m_pntCur.y - m_pntOrigin.y)/float(m_pntCur.x - m_pntOrigin.x));
				}

				EndAppendPoint();
			}
			break;
		case typeMouseMove:
			m_pntPrev = m_pntCur;
			m_pntCur = point;
			if(m_bIsDrag)
			{
				SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
				SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
				if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,false,pStyle,R2_NOTXORPEN))
				{
					fpts[0].x = m_pntOrigin.x;
					fpts[0].y = m_pntOrigin.y;

					fpts[1].x = m_pntPrev.x;
					fpts[1].y = m_pntPrev.y;
					m_pRenderDevice->DrawLine(fpts,2,true);

					fpts[1].x = m_pntCur.x;
					fpts[1].y = m_pntCur.y;
					m_pRenderDevice->DrawLine(fpts,2,true);

					m_pRenderDevice->EndRender(MRD_BL_QUICK);
				}
			}
			break;
		default:break;
		}
	}

	void SmtInputPointTool::AppendDot(short mouse_status,Smt_Core::lPoint point)
	{
		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				m_pntOrigin = point;
				m_pntPrev = point;
				m_pntCur = point;

				float x,y;
				m_pRenderDevice->DPToLP(m_pntOrigin.x,m_pntOrigin.y,x,y);
				m_pGeom = new SmtPoint(x,y);
				SetOperDone(false);
				SetEnableContexMenu(false);
				EndAppendPoint();
			}	
			break;
		}
	}

	void SmtInputPointTool::EndAppendPoint()
	{
		//
		m_pRenderDevice->Refresh();

		ushort		uRetType = GT_MSG_RET_INPUT_POINT;
		SmtListenerMsg param;

		param.hSrcWnd = m_hWnd;
		param.wParam = WPARAM(m_pGeom);
		param.lParam = LPARAM(&uRetType);

		EndIA(GT_MSG_RET_DELEGATE,param);

		SMT_SAFE_DELETE(m_pGeom);

		SetOperDone(true);
	}
}