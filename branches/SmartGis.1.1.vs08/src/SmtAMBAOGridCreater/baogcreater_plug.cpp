#include "stdafx.h"
#include "SmtAMBAOGridCreater.h"
#include "baogcreater_plug.h"
#include "smt_api.h"
#include "smt_gui_api.h"

#include "cata_mapmgr.h"
#include "sys_sysmanager.h"
#include "bl_stylemanager.h"
#include "gt_defs.h"
#include "am_msg.h"
#include "t_msg.h"
#include "smt_listenermanager.h"
#include "t_iatoolmanager.h"
#include "vw_2deditxview.h"
#include "gt_defs.h"

using namespace Smt_GIS;
using namespace Smt_Sys;
using namespace Smt_Base;
using namespace Smt_XView;
using namespace Smt_XCatalog;

const string								CST_STR_BAOGRIDCREATER_PLUG_NAME	= "边界适应正交网格";
SmtBAOGridCreaterPlugin						*g_pBAOGridCreater = NULL;

#define  AM_MSG_CMD_BAOGRIDCREATER_BEGIN		(SMT_MSG_USER_BEGIN+50)
#define  BAOGRIDCREATER_INPUT_BOUDARY0			(AM_MSG_CMD_BAOGRIDCREATER_BEGIN+1)
#define  BAOGRIDCREATER_INPUT_BOUDARY2			(AM_MSG_CMD_BAOGRIDCREATER_BEGIN+2)
#define  BAOGRIDCREATER_SAVE_BOUDARY			(AM_MSG_CMD_BAOGRIDCREATER_BEGIN+3)
#define  BAOGRIDCREATER_LOAD_BOUDARY			(AM_MSG_CMD_BAOGRIDCREATER_BEGIN+4)
#define  AM_MSG_CMD_BAOGRIDCREATER_END			(AM_MSG_CMD_BAOGRIDCREATER_BEGIN + 50)

extern "C"
{
	int SMT_EXPORT_DLL GetPluginVersion(void)
	{
		return 1;
	}

	void SMT_EXPORT_DLL StartPlugin(void)
	{
		g_pBAOGridCreater = new SmtBAOGridCreaterPlugin();
		if (g_pBAOGridCreater)
		{
			g_pBAOGridCreater->Init();
		}
	}

	void SMT_EXPORT_DLL StopPlugin(void)
	{
		if (g_pBAOGridCreater)
		{
			g_pBAOGridCreater->Destroy();
		}

		SMT_SAFE_DELETE(g_pBAOGridCreater);
	}
}


SmtBAOGridCreaterPlugin::SmtBAOGridCreaterPlugin(void):m_pActiveTool(NULL)
	,m_pRenderDevice(NULL)
	,m_p2DEditView(NULL)
{
	SetName(CST_STR_BAOGRIDCREATER_PLUG_NAME.c_str());
}
	
SmtBAOGridCreaterPlugin::~SmtBAOGridCreaterPlugin(void)
{
	SMT_SAFE_DELETE(m_pActiveTool);
}

int SmtBAOGridCreaterPlugin::Init(void)
{
	SmtAuxModule::Init();

	AppendFuncItems("输入0号边界",BAOGRIDCREATER_INPUT_BOUDARY0,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("输入2号边界",BAOGRIDCREATER_INPUT_BOUDARY2,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("保存网格边界",BAOGRIDCREATER_SAVE_BOUDARY,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	AppendFuncItems("导入网格边界",BAOGRIDCREATER_LOAD_BOUDARY,FIM_2DMFMENU|FIM_AUXMODULEBOX);
	
	RegisterMsg();

	return SMT_ERR_NONE;
}

int SmtBAOGridCreaterPlugin::Destroy(void)
{
	return SmtAuxModule::Destroy();
}

int SmtBAOGridCreaterPlugin::Notify(long lMsg,SmtListenerMsg &param)
{
	if (lMsg < AM_MSG_CMD_BAOGRIDCREATER_BEGIN  ||
		lMsg > AM_MSG_CMD_BAOGRIDCREATER_END)
		return SMT_ERR_NONE;
	 
	if (SMT_ERR_NONE != Init2DStuff())
		return SMT_ERR_FAILURE;

	switch (lMsg)
	{
	case BAOGRIDCREATER_INPUT_BOUDARY0:
		{
			m_ctrlBnd0.clear();
			m_ctrlBnd2.clear();
			m_bndIndex = 0;

			if (SMT_ERR_NONE == CreateIAGetLineTool())
			{
				ushort unType = LT_LineString;
				SmtListenerMsg param0;
				param0.hSrcWnd = m_pActiveTool->GetOwnerWnd();
				param0.wParam = WPARAM(&unType);
				m_pActiveTool->Notify(GT_MSG_SET_INPUT_LINE_TYPE,param0);
				m_pActiveTool->SetActive();
			}
		}
		break;
	case BAOGRIDCREATER_INPUT_BOUDARY2:
		{
			if (m_bndIndex != 0)
				return SMT_ERR_FAILURE;
			 
			m_bndIndex = 2;

			if (SMT_ERR_NONE == CreateIAGetLineTool())
			{
				ushort unType = LT_LineString;
				SmtListenerMsg param0;
				param0.hSrcWnd = m_pActiveTool->GetOwnerWnd();
				param0.wParam = WPARAM(&unType);
				m_pActiveTool->Notify(GT_MSG_SET_INPUT_LINE_TYPE,param0);
				m_pActiveTool->SetActive();
			}
		}
		break;
	case BAOGRIDCREATER_SAVE_BOUDARY:
		{
			;
		}
		break;
	case BAOGRIDCREATER_LOAD_BOUDARY:
		{
			LoadFromFile();
		}
		break;
	}
	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
int SmtBAOGridCreaterPlugin::Init2DStuff(void)
{ 
	SmtMapMgr		*pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
	SmtLayer		*pLayer = pSmtMapMgr->GetActiveLayer();
	SmtVectorLayer	*pVLayer = NULL;

	if (NULL != pLayer &&
		pLayer->GetLayerType() == LYR_VECTOR)
	{
		SmtVectorLayer	*pVLayer = (SmtVectorLayer*)pLayer;
		if(pVLayer->GetLayerFeatureType() != SmtFtGrid)
		{
			::MessageBox(::GetActiveWindow(), "请激活GRID图层!", "提示", MB_OK);
			return SMT_ERR_FAILURE;
		}
	}
	else
		return SMT_ERR_FAILURE;

	if (NULL != m_p2DEditView &&
		NULL != m_p2DEditView->GetSafeHwnd())
		return SMT_ERR_NONE;

	SmtListenerMsg msgParam;
	msgParam.lParam = (LPARAM)&m_p2DEditView;
	SmtPostListenerMsg(SMT_LISTENER_MSG_BROADCAST,SMT_MSG_GET_SYS_2DEDITVIEW,msgParam);

	if (NULL == m_p2DEditView ||
		NULL == m_p2DEditView->GetSafeHwnd())
		return SMT_ERR_FAILURE;

	m_pRenderDevice = m_p2DEditView->GetRenderDevice();

	if (NULL == m_pRenderDevice)
		return SMT_ERR_FAILURE;

	return SMT_ERR_NONE;
}

int SmtBAOGridCreaterPlugin::CreateIAGetLineTool(void)
{
	SMT_SAFE_DELETE(m_pActiveTool);

	//创建tool
	SmtGroupToolFactory::CreateGroupTool(m_pActiveTool,GroupToolType::GTT_InputLine);

	if (NULL == m_pActiveTool)
		return SMT_ERR_FAILURE;

	SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();

	//设置tool
	m_pActiveTool->SetToolStyleName(pSysMgr->GetSysStyleConfig().szLineStyle);

	if (m_pActiveTool->Init(m_p2DEditView->GetRenderDevice(),m_p2DEditView->GetOperMap(),
		m_p2DEditView->GetSafeHwnd(),GetIAToolResult,(void*)this) != SMT_ERR_NONE)
		return SMT_ERR_FAILURE;

	return SMT_ERR_NONE;
}

int SmtBAOGridCreaterPlugin::GetIAToolResult(long nMsg,SmtListenerMsg &param)
{
	SmtBAOGridCreaterPlugin *pThis = (SmtBAOGridCreaterPlugin *)param.pToFollow;

	ushort uRetType = *(ushort*)param.lParam;

	if (uRetType != GT_MSG_RET_INPUT_LINE)
		return SMT_ERR_FAILURE;

	pThis->OnEndInputBnd((SmtLineString *)param.wParam);

	return SMT_ERR_NONE;
}

int SmtBAOGridCreaterPlugin::OnEndInputBnd(SmtLineString *pLineString)
{
	if (m_bndIndex == 0)
	{
		int nNum = pLineString->GetNumPoints();
		m_ctrlBnd0.resize(nNum);
		for (int i = 0; i < nNum;i++)
		{
			m_ctrlBnd0[i].x = pLineString->GetX(i);
			m_ctrlBnd0[i].y = pLineString->GetY(i);
		}
	}
	else if (m_bndIndex == 2)
	{
		int nNum = pLineString->GetNumPoints();
		m_ctrlBnd2.resize(nNum);
		for (int i = 0; i < nNum;i++)
		{
			m_ctrlBnd2[i].x = pLineString->GetX(i);
			m_ctrlBnd2[i].y = pLineString->GetY(i);
		}

		m_bndIndex = -1;

		vdbfPoints ctrlBnd1;
		vdbfPoints ctrlBnd3;

		ctrlBnd1.push_back(m_ctrlBnd0[m_ctrlBnd0.size()-1]);
		ctrlBnd1.push_back(m_ctrlBnd2[0]);

		ctrlBnd3.push_back(m_ctrlBnd2[m_ctrlBnd2.size()-1]);
		ctrlBnd3.push_back(m_ctrlBnd0[0]);

		SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
		SmtLayer  *pLayer = pSmtMapMgr->GetActiveLayer();
		SmtVectorLayer *pVLayer = NULL;
		if (NULL == pLayer || 
			LYR_VECTOR != pLayer->GetLayerType() )
			return SMT_ERR_FAILURE;

		pVLayer = (SmtVectorLayer *)pLayer;
		 
		if(NULL != pVLayer && 
		   SmtFtGrid == pVLayer->GetLayerFeatureType())
		{
			SmtGrid		 oSmtGrid;
			SmtBAOrthGrid orthGrid(33,17);

			orthGrid.SetMainRegoinBoudary(m_ctrlBnd0,ctrlBnd1,m_ctrlBnd2,ctrlBnd3);

			if (SMT_ERR_NONE == orthGrid.CreateOrthGrid() &&
				SMT_ERR_NONE == orthGrid.CvtToGrid(oSmtGrid))
			{
				string strAppTempPath = GetAppTempPath();
				strAppTempPath += "last_bfc_bnd.txt";
				orthGrid.SaveGridBndToFile(strAppTempPath.c_str());

				SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
				SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

				SmtFeature *pSmtFeature = new SmtFeature;

				pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
				pSmtFeature->SetStyle(styleSonfig.szPointStyle);
				pSmtFeature->SetGeometry(&oSmtGrid);

				if (pSmtMapMgr->AppendFeature(pSmtFeature,false))
				{
					SmtListenerMsg param;
					::MessageBox(::GetActiveWindow(), "生成成功!", "提示", MB_OK);
					SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,GT_MSG_VIEW_ZOOMREFRESH,param);
				}
				else
					SMT_SAFE_DELETE(pSmtFeature);
			}
		}	
	}
	
	return SMT_ERR_NONE;
}

void SmtBAOGridCreaterPlugin::LoadFromFile(void)
{
	SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
	SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

	if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
		return;

	SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;
	 
	if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtGrid)
	{
		static char BASED_CODE szFilter[] = "Data Files (*.txt)|*.txt" ;

		CFileDialog dlg( true , NULL , NULL , OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT , szFilter , NULL ) ;

		if( dlg.DoModal() == IDCANCEL )
			return;

		SmtGrid     oSmtGrid;
		SmtBAOrthGrid orthGrid;

		if (SMT_ERR_NONE == orthGrid.LoadGridBndFromFile(dlg.GetPathName())&&
			SMT_ERR_NONE == orthGrid.CreateOrthGrid() &&
			SMT_ERR_NONE == orthGrid.CvtToGrid(oSmtGrid))
		{
			string strAppTempPath = GetAppTempPath();
			strAppTempPath += "last_bfc_bnd.txt";
			orthGrid.SaveGridBndToFile(strAppTempPath.c_str());

			SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
			SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

			SmtFeature *pSmtFeature = new SmtFeature;

			pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
			pSmtFeature->SetStyle(styleSonfig.szPointStyle);
			pSmtFeature->SetGeometry(&oSmtGrid);

			if (pSmtMapMgr->AppendFeature(pSmtFeature,false))
			{
				SmtListenerMsg param;
				::MessageBox(::GetActiveWindow(), "生成成功!", "提示", MB_OK);
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,GT_MSG_VIEW_ZOOMREFRESH,param);
			}
			else
				SMT_SAFE_DELETE(pSmtFeature);
		}
	}
}