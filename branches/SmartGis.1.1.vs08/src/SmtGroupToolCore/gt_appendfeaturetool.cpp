#include <math.h>

#include "smt_api.h"
#include "bl_api.h"
#include "gt_appendfeaturetool.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "geo_geometry.h"
#include "gt_defs.h"
#include "gt_grouptoolfactory.h"
#include "sys_sysmanager.h"
#include "smt_gui_api.h"
#include "gt_iacommandreceiver.h"

#include "resource.h "

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Geo;
using namespace Smt_Sys;
using namespace Smt_Base;

const string						CST_STR_APPENDFEATURE_TOOL_NAME	= "添加要素";

namespace Smt_GroupTool
{
	SmtAppendFeatureTool::SmtAppendFeatureTool():m_pGeom(NULL)
		,m_strAnno("Smart GIS")
	{
		 SetName(CST_STR_APPENDFEATURE_TOOL_NAME.c_str());
	}

	SmtAppendFeatureTool::~SmtAppendFeatureTool()
	{
		this->EndDelegate();
		m_pGeom = NULL;

		UnRegisterMsg();
	}

	int SmtAppendFeatureTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		SmtStyle *pStyle = pStyleMgr->GetStyle(m_szStyleName);
		pStyle->SetStyleType(ST_PenDesc|ST_BrushDesc|ST_SymbolDesc|ST_AnnoDesc);

		//
		AppendFuncItems("添加子图",GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加注记",GT_MSG_APPEND_POINT_ANNO_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加点",GT_MSG_APPEND_POINT_DOT_FEATURE,FIM_2DMFMENU);

		//
		AppendFuncItems("添加折线",GT_MSG_APPEND_LINE_LINESTRING_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加拉格朗日曲线",GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加贝奇尔曲线",GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加B样条曲线",GT_MSG_APPEND_LINE_SPLINE_B_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加三次样条曲线",GT_MSG_APPEND_LINE_SPLINE_3_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加矩形线",GT_MSG_APPEND_LINE_RECT_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加弧线",GT_MSG_APPEND_LINE_ARC_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加闭合折线",GT_MSG_APPEND_LINE_LINEARRING_FEATURE,FIM_2DMFMENU);

		//
		AppendFuncItems("添加扇形区",GT_MSG_APPEND_SURF_FAN_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加矩形区",GT_MSG_APPEND_SURF_RECT_FEATURE,FIM_2DMFMENU);
		AppendFuncItems("添加多边形区",GT_MSG_APPEND_SURF_POLYGON_FEATURE,FIM_2DMFMENU);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_ANNO_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_DOT_FEATURE);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_LINESTRING_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_B_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_3_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_RECT_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_ARC_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_LINEARRING_FEATURE);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_FAN_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_RECT_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_POLYGON_FEATURE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtAppendFeatureTool::AuxDraw()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtAppendFeatureTool::Timer()
	{
		return SmtBaseTool::Timer();
	}

	void SmtAppendFeatureTool::SetOperMap(SmtMap *pOperSmtMap)
	{ 
		SmtBaseTool::SetOperMap(pOperSmtMap);

		m_cmdMgr.ClearAllCommands();
	}

	int SmtAppendFeatureTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{

			}
			break;
		case GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE:
			{
				ushort unType = PT_ChildImage;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_POINT_ANNO_FEATURE:
			{
				ushort unType = PT_Text;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_POINT_DOT_FEATURE:
			{
				ushort unType = PT_DOT;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		case GT_MSG_APPEND_LINE_RECT_FEATURE:
			{
				ushort unType = LT_Rect;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_ARC_FEATURE:
			{
				ushort unType = LT_Arc;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_LINEARRING_FEATURE:
			{
				ushort unType = LT_LinearRing;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_LINESTRING_FEATURE:
			{
				ushort unType = LT_LineString;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE:
			{
				ushort unType = LT_Spline_Lag;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE:
			{
				ushort unType = LT_Spline_Bzer;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_B_FEATURE:
			{
				ushort unType = LT_Spline_B;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_3_FEATURE:
			{
				ushort unType = LT_Spline_3;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		case GT_MSG_APPEND_SURF_FAN_FEATURE:
			{
				ushort unType = RT_Fan;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_SURF_RECT_FEATURE:
			{
				ushort unType = RT_Rect;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_SURF_POLYGON_FEATURE:
			{
				ushort unType = RT_Polygon;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		case GT_MSG_RET_DELEGATE:
			{
				ushort uRetType = *(ushort*)param.lParam;
				m_pGeom = ((SmtGeometry *)param.wParam)->Clone();
				OnRetDelegate(uRetType);
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	void SmtAppendFeatureTool::OnRetDelegate(int nRetType)
	{
		switch (nRetType)
		{
		case GT_MSG_RET_INPUT_POINT:
			{
				ushort unPointType = 0;
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(&unPointType);
				m_pDelegateTag->Notify(GT_MSG_GET_INPUT_POINT_TYPE,param);
				AppendPointFeature(unPointType);
			}
			break;
		case GT_MSG_RET_INPUT_LINE:
			{
				AppendLineFeature();
			}
			break;
		case GT_MSG_RET_INPUT_REGION:
			{
				AppendRegionFeature();
			}
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtAppendFeatureTool::OnInputPointFeature(ushort unType)
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		this->EndDelegate();

		SmtBaseTool *pInputTool = NULL;
		SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputPoint);

		if(NULL != pInputTool)
		{
			pInputTool->SetToolStyleName(styleSonfig.szPointStyle);

			if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(&unType);
				pInputTool->Notify(GT_MSG_SET_INPUT_POINT_TYPE,param);
				this->BeginDelegate(pInputTool);	
			}
		}
		
		SetActive();
	}

	void SmtAppendFeatureTool::OnInputLineFeature(ushort unType)
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		this->EndDelegate();

		SmtBaseTool *pInputTool = NULL;
		SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputLine);

		if(NULL != pInputTool)
		{
			pInputTool->SetToolStyleName(styleSonfig.szLineStyle);

			if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(&unType);

				pInputTool->Notify(GT_MSG_SET_INPUT_LINE_TYPE,param);

				this->BeginDelegate(pInputTool);
			}
		}

		SetActive();
	}

	void SmtAppendFeatureTool::OnInputRegionFeature(ushort unType)
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		this->EndDelegate();

		SmtBaseTool *pInputTool = NULL;
		SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputRegion);

		if(NULL != pInputTool)
		{
			pInputTool->SetToolStyleName(styleSonfig.szRegionStyle);

			if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
			{
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(&unType);

				pInputTool->Notify(GT_MSG_SET_INPUT_REGION_TYPE,param);

				this->BeginDelegate(pInputTool);
			}
		}
	
		SetActive();
	}
	//////////////////////////////////////////////////////////////////////////
	void SmtAppendFeatureTool::AppendPointFeature(ushort unType)
	{
		switch (unType)
		{
		case PT_ChildImage:
			{
				AppendChildImageFeature();
			}
			break;
		case PT_Text:
			{
				float angle = 0;
				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(&angle);
				m_pDelegateTag->Notify(GT_MSG_GET_INPUT_ANNO_ANGLE,param);
				if (SMT_ERR_NONE == SmtInputTextDlg(m_strAnno))
					AppendTextFeature(m_strAnno.c_str(),angle);
			}
			break;
		case PT_DOT:
			{
				AppendDotFeature();
			}
			break;
		}
	}

	void SmtAppendFeatureTool::AppendChildImageFeature()
	{
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature *pSmtFeature = new SmtFeature;
			pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtChildImage);
			pSmtFeature->SetStyle(styleSonfig.szPointStyle);
			pSmtFeature->SetGeometryDirectly(m_pGeom);

			if (m_pOperMap->AppendFeature(pSmtFeature,false))
			{
				fRect frt;
				Envelope envelope;
				SmtPoint * pPoint = (SmtPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				m_pGeom->GetEnvelope(&envelope);
				envelope.Merge(pPoint->GetX()-fMargin,pPoint->GetY()-fMargin);
				envelope.Merge(pPoint->GetX()+fMargin,pPoint->GetY()+fMargin);
				EnvelopeToRect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);

				//
				SmtCommand  *pCommand = new SmtCommand(new SmtIACommandReceiver(m_pRenderDevice,m_pOperMap,pSmtFeature,eSmtIA_AppendFeature));
				m_cmdMgr.PushUndoCommand(pCommand);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);

			m_pGeom = NULL;
		}
	}

	void SmtAppendFeatureTool::AppendTextFeature(const char * szAnno,float fangle)
	{	 
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature *pSmtFeature = new SmtFeature;
	
			pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtAnno);
			pSmtFeature->SetStyle(styleSonfig.szPointStyle);
			pSmtFeature->SetGeometryDirectly(m_pGeom);
			pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),szAnno);
			pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
			pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),fangle);

			if (m_pOperMap->AppendFeature(pSmtFeature,false))
			{
				fRect frt;
				Envelope envelope;
				SmtPoint * pPoint = (SmtPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				m_pGeom->GetEnvelope(&envelope);
				envelope.Merge(pPoint->GetX()-fMargin,pPoint->GetY()-fMargin);
				envelope.Merge(pPoint->GetX()+fMargin,pPoint->GetY()+fMargin);
				EnvelopeToRect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);

				//
				SmtCommand  *pCommand = new SmtCommand(new SmtIACommandReceiver(m_pRenderDevice,m_pOperMap,pSmtFeature,eSmtIA_AppendFeature));
				m_cmdMgr.PushUndoCommand(pCommand);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);

			m_pGeom = NULL;
		}
	}

	void  SmtAppendFeatureTool::AppendDotFeature()
	{
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature *pSmtFeature = new SmtFeature;

			pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtDot);
			pSmtFeature->SetStyle(styleSonfig.szPointStyle);
			pSmtFeature->SetGeometryDirectly(m_pGeom);

			if (m_pOperMap->AppendFeature(pSmtFeature,false))
			{
				fRect frt;
				Envelope envelope;
				SmtPoint * pPoint = (SmtPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();
				
				m_pGeom->GetEnvelope(&envelope);
				envelope.Merge(pPoint->GetX()-fMargin,pPoint->GetY()-fMargin);
				envelope.Merge(pPoint->GetX()+fMargin,pPoint->GetY()+fMargin);
				EnvelopeToRect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);

				//
				SmtCommand  *pCommand = new SmtCommand(new SmtIACommandReceiver(m_pRenderDevice,m_pOperMap,pSmtFeature,eSmtIA_AppendFeature));
				m_cmdMgr.PushUndoCommand(pCommand);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);

			m_pGeom = NULL;
		}
	}

	void SmtAppendFeatureTool::AppendLineFeature(void)
	{	
		if(m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature * pSmtFeature = new SmtFeature;	
			pSmtFeature->SetFeatureType(SmtFtCurve);
			pSmtFeature->SetStyle(styleSonfig.szLineStyle);
			pSmtFeature->SetGeometryDirectly(m_pGeom);
			pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)m_pGeom)->GetLength());

			if (m_pOperMap->AppendFeature(pSmtFeature,false))
			{
				fRect frt;
				Envelope envelope ;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				m_pGeom->GetEnvelope(&envelope);
				envelope.Merge(envelope.MinX-fMargin,envelope.MinY-fMargin);
				envelope.Merge(envelope.MaxX+fMargin,envelope.MaxY+fMargin);
				EnvelopeToRect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);

				//
				SmtCommand  *pCommand = new SmtCommand(new SmtIACommandReceiver(m_pRenderDevice,m_pOperMap,pSmtFeature,eSmtIA_AppendFeature));
				m_cmdMgr.PushUndoCommand(pCommand);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);

			m_pGeom = NULL;
		}
	}

	void SmtAppendFeatureTool::AppendRegionFeature()
	{
		if (m_pOperMap)
		{	
			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature * pSmtFeature = new SmtFeature;
			pSmtFeature->SetFeatureType(SmtFtSurface);
			pSmtFeature->SetStyle(styleSonfig.szRegionStyle);
			pSmtFeature->SetGeometryDirectly(m_pGeom);
			pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("area"),((SmtSurface*)m_pGeom)->GetArea());

			if (m_pOperMap->AppendFeature(pSmtFeature,false))
			{
				fRect frt;
				Envelope envelope ;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				m_pGeom->GetEnvelope(&envelope);
				envelope.Merge(envelope.MinX-fMargin,envelope.MinY-fMargin);
				envelope.Merge(envelope.MaxX+fMargin,envelope.MaxY+fMargin);
				EnvelopeToRect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);

				//
				SmtCommand  *pCommand = new SmtCommand(new SmtIACommandReceiver(m_pRenderDevice,m_pOperMap,pSmtFeature,eSmtIA_AppendFeature));
				m_cmdMgr.PushUndoCommand(pCommand);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);

			m_pGeom = NULL;
		}
	}

	int SmtAppendFeatureTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		if(m_pDelegateTag != NULL && !m_pDelegateTag->IsOperDone())
			return m_pDelegateTag->KeyDown(nChar,nRepCnt,nFlags);
		else
		{
			//if (GetKeyState(VK_CONTROL) & 0x80000000)
			{
				switch (nChar)
				{
				case 't':
				case 'T':
					{
						if (m_cmdMgr.CanUndo())
							m_cmdMgr.Undo();
					}
					break;
				case 'y':
				case 'Y':
					{
						if (m_cmdMgr.CanRedo())
							m_cmdMgr.Redo();
					}
					break;
				}
			}
		}

		return SmtBaseTool::KeyDown(nChar,nRepCnt,nFlags);
	}
}