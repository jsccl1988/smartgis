#include <math.h>

#include "smt_api.h"
#include "gt_inputregiontool.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "geo_geometry.h"
#include "geo_api.h"
#include "gt_defs.h"
#include "sys_sysmanager.h"
#include "bl_api.h"

#include "resource.h "

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Geo;
using namespace Smt_Sys;

const string						CST_STR_INPUTREGION_TOOL_NAME	= "ÊäÈëµã";

namespace Smt_GroupTool
{
	SmtInputRegionTool::SmtInputRegionTool():m_pGeom(NULL)
		,m_bIsDrag(false)
		,m_bDelay(false)
		,m_appendType(-1)
		,m_nStep(0)
	{
		SetName(CST_STR_INPUTREGION_TOOL_NAME.c_str());
	}

	SmtInputRegionTool::~SmtInputRegionTool()
	{
		SMT_SAFE_DELETE(m_pGeom);

		UnRegisterMsg();
	}

	int SmtInputRegionTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);

		pStyle->SetStyleType(ST_PenDesc|ST_BrushDesc);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_INPUT_REGION_TYPE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_INPUT_REGION_TYPE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::AuxDraw()
	{
		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{
				;
			}
			break;
		case GT_MSG_SET_INPUT_REGION_TYPE:
			{
				m_appendType = *(ushort*)param.wParam;
			}
			break;
		case GT_MSG_GET_INPUT_REGION_TYPE:
			{
				*(ushort*)param.wParam = m_appendType;
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::LButtonDown(uint nFlags, lPoint point)
	{
		SetOperDone(false);

		switch(m_appendType)
		{
		case RT_Fan:
			AppendFan(typeLButtonDown,point);
			break;
		case RT_Rect:
			AppendRect(typeLButtonDown,point);
			break;
		case RT_Polygon:
			AppendPolygon(typeLButtonDown,point);
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::MouseMove(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case RT_Fan:
			AppendFan(typeMouseMove,point);
			break;
		case RT_Rect:
			AppendRect(typeMouseMove,point);
			break;
		case RT_Polygon:
			AppendPolygon(typeMouseMove,point);
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::LButtonUp(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case RT_Fan:
			AppendFan(typeLButtonUp,point);
			break;
		case RT_Rect:
			AppendRect(typeLButtonUp,point);
			break;
		case RT_Polygon:
			AppendPolygon(typeLButtonUp,point);
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	int SmtInputRegionTool::RButtonDown(uint nFlags, lPoint point)
	{
		switch(m_appendType)
		{
		case RT_Fan:
			AppendFan(typeRButtonDown,point);
			break;
		case RT_Rect:
			AppendRect(typeRButtonDown,point);
			break;
		case RT_Polygon:
			AppendPolygon(typeRButtonDown,point);
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

	int SmtInputRegionTool::MouseWeel(uint nFlags, short zDelta, lPoint point) 
	{	
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtInputRegionTool::AppendFan(uint mouse_status,lPoint point)
	{
		float x1,y1;
		static fPoint fpts[2];

		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				if (!m_bIsDrag )
				{
					m_pntOrigin = point;
					m_pntPrev = point;
					m_bIsDrag = true;
					m_nStep = 1;
					SmtFan *pFan =  new SmtFan();	
					m_pGeom = pFan;
					pFan->SetArcDirectly(new SmtArc());

					SetOperDone(false);
					SetEnableContexMenu(false);
				}

				m_pntPrev = m_pntCur;
				m_pntCur  = point;

				m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);
				SmtArc *pArc = ((SmtFan *)m_pGeom)->GetArc();
				if (m_nStep == 1)
					pArc->SetCenterPoint(RawPoint(x1,y1));
				else if (m_nStep == 2)
				{
					SmtPoint oCtPoint;

					pArc->GetCenterPoint(&oCtPoint);
					pArc->SetStartPoint(RawPoint(x1,y1));
					pArc->SetRadius(SmtDistance(oCtPoint.GetX(),oCtPoint.GetY(),x1,y1));
				}
				else if (m_nStep == 3)
				{
					m_bIsDrag = false;
					m_nStep = 1;

					EndAppendRegion();

					if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,true,false))
						m_pRenderDevice->EndRender(MRD_BL_QUICK);
				}

				m_nStep ++;
			}	
			break;
		case typeMouseMove:

			m_pntPrev = m_pntCur;
			m_pntCur = point;

			if(m_bIsDrag)
			{
				SmtArc *pArc = ((SmtFan *)m_pGeom)->GetArc();
				if (m_nStep == 3)
				{
					SmtPoint oCtPoint,oStPoint;
					pArc->GetCenterPoint(&oCtPoint);
					pArc->StartPoint(&oStPoint);
					//
					m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);
					float R = pArc->GetRadius();
					float r = SmtDistance(oCtPoint.GetX(),oCtPoint.GetY(),x1,y1);

					//
					x1 = (x1-oCtPoint.GetX())*R/r+oCtPoint.GetX();
					y1 = (y1-oCtPoint.GetY())*R/r+oCtPoint.GetY();

					//
					pArc->SetEndPoint(RawPoint(x1,y1));
					m_pRenderDevice->Refresh();
				}

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

	void SmtInputRegionTool::AppendRect(uint mouse_status,lPoint point)
	{
		float x1,y1;
		static lPoint lpts[2];

		switch (mouse_status)
		{
		case typeLButtonDown:
			{
				m_pntOrigin = point;
				m_pntPrev   = point;
				m_pntCur    = point;	
				m_bIsDrag   = true;

				SetOperDone(false);
				SetEnableContexMenu(false);

				SmtLinearRing *pLineaRing = new SmtLinearRing();	
				m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);
				pLineaRing->AddPoint(x1,y1);
				m_pGeom = pLineaRing;
			}
			break;
		case typeLButtonUp:
			if(m_bIsDrag)
			{			
				m_bIsDrag = false;
				m_pntCur = point;
				
				SmtLinearRing *pLinearRing = (SmtLinearRing *)m_pGeom;
				SmtPolygon *pPolygon =  new SmtPolygon();
				pPolygon->AddRingDirectly(pLinearRing);
				m_pGeom = pPolygon;
			
				EndAppendRegion();

				if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,true,false))
					m_pRenderDevice->EndRender(MRD_BL_QUICK);
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
					fRect frt1,frt2;
					frt1.Merge(m_pntOrigin.x,m_pntOrigin.y);
					frt1.Merge(m_pntPrev.x,m_pntPrev.y);
					m_pRenderDevice->DrawRect(frt1,true);

					frt2.Merge(m_pntOrigin.x,m_pntOrigin.y);
					frt2.Merge(m_pntCur.x,m_pntCur.y);
					m_pRenderDevice->DrawRect(frt2,true);

					m_pRenderDevice->EndRender(MRD_BL_QUICK);
				}

				float x1,y1,x2,y2;
				SmtLinearRing *pLinearRing = (SmtLinearRing *)m_pGeom;	
				m_pRenderDevice->DPToLP(m_pntOrigin.x,m_pntOrigin.y,x1,y1);
				m_pRenderDevice->DPToLP(m_pntCur.x,m_pntCur.y,x2,y2);
				pLinearRing->SetPoint(1,x2,y1);
				pLinearRing->SetPoint(2,x2,y2);
				pLinearRing->SetPoint(3,x1,y2);
				pLinearRing->CloseRings();
			}
			break;
		default:break;
		}
	}

	void SmtInputRegionTool::AppendPolygon(uint mouse_status,lPoint point)
	{
		float x1,y1;
		static fPoint fpts[2];

		switch (mouse_status)
		{
		case typeLButtonDown:

			m_pntOrigin = point;
			m_pntPrev = point;
			m_pntCur = point;

			if (!m_bIsDrag)
			{
				SmtLinearRing *pLinearRing = new SmtLinearRing();	
				m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);
				pLinearRing->AddPoint(x1,y1);
				m_pGeom = pLinearRing;
				m_bIsDrag = true;
				m_nStep = 0;
				SetOperDone(false);
				SetEnableContexMenu(false);
			}
			else
			{
				SmtLinearRing *pLinearRing = (SmtLinearRing *)m_pGeom;
				m_pRenderDevice->DPToLP(point.x,point.y,x1,y1);
				pLinearRing->AddPoint(x1,y1);
			}

			m_nStep ++;

			break;
		case typeRButtonDown:
			if(m_bIsDrag)
			{	
				if (m_nStep < 3)
					MessageBeep(0xFFFFFFFF);
				else
				{
					m_bIsDrag = 0;
					m_pntCur = point;
					m_bDelay = true;
					m_nStep = 0;

					SmtLinearRing *pLinearRing = (SmtLinearRing *)m_pGeom;
					pLinearRing->CloseRings();

					SmtPolygon *pPolygon =  new SmtPolygon();
					pPolygon->AddRingDirectly(pLinearRing);
					m_pGeom = pPolygon;
					EndAppendRegion();

					if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_QUICK,true,false))
						m_pRenderDevice->EndRender(MRD_BL_QUICK);
				}		
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

	void SmtInputRegionTool::EndAppendRegion()
	{
		m_pRenderDevice->Refresh();

		ushort		uRetType = GT_MSG_RET_INPUT_REGION;
		SmtListenerMsg param;

		param.hSrcWnd = m_hWnd;
		param.wParam = WPARAM(m_pGeom);
		param.lParam = LPARAM(&uRetType);

		EndIA(GT_MSG_RET_DELEGATE,param);

		SMT_SAFE_DELETE(m_pGeom);
		SetOperDone(true);
	}
}