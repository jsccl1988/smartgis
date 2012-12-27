#include <math.h>
#include <assert.h>

#include "resource.h "
#include "gt_selecttool.h"
#include "smt_api.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "sde_datasourcemgr.h"
#include "sys_sysmanager.h"
#include "gt_grouptoolfactory.h"
#include "t_iatoolmanager.h"
#include "smt_gui_api.h"
#include "smt_logmanager.h"

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Base;
using namespace Smt_Sys;
using namespace Smt_SDEDevMgr;

const string						CST_STR_SELECT_TOOL_NAME	= "选取";
const string						C_STR_SELECT_TOO_LOG = "SmtSelectTool";

namespace Smt_GroupTool
{
	SmtSelectTool::SmtSelectTool():m_bCaptured(false)
		,m_pResultLayer(NULL)
	{
		SetName(CST_STR_SELECT_TOOL_NAME.c_str());
	}

	SmtSelectTool::~SmtSelectTool()
	{
		SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

		this->EndDelegate();

		SmtDataSourceMgr::DestoryMemVecLayer(m_pResultLayer);

		UnRegisterMsg();
	}

	int SmtSelectTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_SELECT_TOO_LOG);
		if (NULL == pLog)
		{
			pLogMgr->CreateLog(C_STR_SELECT_TOO_LOG.c_str());
		}

		m_pResultLayer = SmtDataSourceMgr::CreateMemVecLayer();
		m_pResultLayer->Open("");

		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra = pSysMgr->GetSysPra();

		m_dpMargin = sysPra.fSmargin;

		AppendFuncItems("点选",GT_MSG_SELECT_POINTSEL,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("框选",GT_MSG_SELECT_RECTSEL,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("多边形选取",GT_MSG_SELECT_POLYGONSEL,FIM_2DVIEW|FIM_2DMFMENU);
		AppendFuncItems("清除选择要素",GT_MSG_SELECT_CLEAR,FIM_2DVIEW|FIM_2DMFMENU);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POINTSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_RECTSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POLYGONSEL);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_CLEAR);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SEL_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SEL_MODE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtSelectTool::AuxDraw()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtSelectTool::Timer()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtSelectTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{

			}
			break;
		case GT_MSG_SELECT_POINTSEL:
			{
				m_selMode = ST_Point;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_RECTSEL:
			{
				m_selMode = ST_Rect;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_POLYGONSEL:
			{
				m_selMode = ST_Polygon;

				OnSetSelMode();
				param.bModify = true;

				SetActive();
			}
			break;
		case GT_MSG_SELECT_CLEAR:
			{
				m_pResultLayer->DeleteAll();

				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(m_pResultLayer);
				param.lParam = LPARAM(&m_nLayerFeaType);
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_SET_FLASH_DATA),param);

				SetActive();
			}
			break;

		case GT_MSG_SET_SEL_MODE:
			{
				m_selMode = eSelectMode(*(ushort*)param.wParam);

				OnSetSelMode();

				param.bModify = true;
			}
			break;
		case GT_MSG_GET_SEL_MODE:
			{
				*(ushort*)param.wParam = m_selMode;
			}
			break;
		case GT_MSG_RET_DELEGATE:
			{
				ushort uRetType = *(ushort*)param.lParam;
				m_gQDes.pQueryGeom = ((SmtGeometry *)param.wParam)->Clone();

				OnRetDelegate(uRetType);
			}
			break;
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	void SmtSelectTool::OnRetDelegate(int nRetType)
	{
		switch (nRetType)
		{
		case GT_MSG_RET_INPUT_POINT:
			{
				if (!(GetAsyncKeyState( VK_LCONTROL ) & 0x8000))
					m_pResultLayer->DeleteAll();

				m_gQDes.fSmargin = m_dpMargin/m_pRenderDevice->GetBlc();
				m_pOperMap->QueryFeature(&m_gQDes,&m_pQDes,m_pResultLayer,m_nLayerFeaType);

				SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(m_pResultLayer);
				param.lParam = LPARAM(&m_nLayerFeaType);
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_SET_FLASH_DATA),param);

				param.wParam = param.lParam = NULL;
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_START_FLASH),param);

				if (m_pResultLayer->GetFeatureCount() == 1)
				{
					SmtShow2DFeatureInfoDlg(m_pResultLayer->GetFeature(0));
				}
			}
			break;
		case GT_MSG_RET_INPUT_LINE:
			{
				if (!(GetAsyncKeyState( VK_LCONTROL ) & 0x8000))
					m_pResultLayer->DeleteAll();

				m_gQDes.fSmargin = m_dpMargin/m_pRenderDevice->GetBlc();
				m_pOperMap->QueryFeature(&m_gQDes,&m_pQDes,m_pResultLayer,m_nLayerFeaType);

				SMT_SAFE_DELETE(m_gQDes.pQueryGeom);	

				SmtListenerMsg param;
				param.hSrcWnd = m_hWnd;
				param.wParam = WPARAM(m_pResultLayer);
				param.lParam = LPARAM(&m_nLayerFeaType);
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_SET_FLASH_DATA),param);

				param.wParam = param.lParam = NULL;
				SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_START_FLASH),param);

				uint unID = SMT_C_INVALID_UINT_VALUE;

				if (m_pResultLayer->GetFeatureCount() > 1)
				{
					vector<uint> vIDs;

					m_pResultLayer->MoveFirst();
					while (!m_pResultLayer->IsEnd())
					{
						SmtFeature *pSmtFea = m_pResultLayer->GetFeature();
						vIDs.push_back((uint)pSmtFea->GetID());

						m_pResultLayer->MoveNext();
					}

					SmtSelectOneDlg(unID,vIDs);
					SmtShow2DFeatureInfoDlg(m_pResultLayer->GetFeatureByID(unID));
				}
				else if (m_pResultLayer->GetFeatureCount() == 1)
				{
					m_pResultLayer->MoveFirst();

					SmtShow2DFeatureInfoDlg(m_pResultLayer->GetFeature());
				}
			}
			break;
		case GT_MSG_RET_INPUT_REGION:
			{
				
			}
			break;
		}
	}

	void SmtSelectTool::OnSetSelMode(void)
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();

		this->EndDelegate();

		SmtBaseTool *pInputTool = NULL;
		
		switch (m_selMode)
		{
		case ST_Point:
			{
				SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputPoint);

				if(NULL != pInputTool)
				{
					pInputTool->SetToolStyleName(styleSonfig.szLineStyle);

					if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
					{
						ushort unPointType = PT_DOT;

						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;
						param.wParam = WPARAM(&unPointType);
						param.lParam = NULL;
						pInputTool->Notify(GT_MSG_SET_INPUT_POINT_TYPE,param);

						this->BeginDelegate(pInputTool);
					}
				}
			}
			break;
		case ST_Polygon:
			{
				SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputLine);

				if(NULL != pInputTool)
				{
					pInputTool->SetToolStyleName(styleSonfig.szLineStyle);

					if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
					{
						ushort unLineType = LT_LinearRing;

						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;
						param.wParam = WPARAM(&unLineType);
						param.lParam = NULL;

						pInputTool->Notify(GT_MSG_SET_INPUT_LINE_TYPE,param);

						this->BeginDelegate(pInputTool);
					}
				}
			}
			break;
		case ST_Rect:
			{
				SmtGroupToolFactory::CreateGroupTool(pInputTool,GroupToolType::GTT_InputLine);

				if(NULL != pInputTool)
				{
					pInputTool->SetToolStyleName(styleSonfig.szLineStyle);

					if (SMT_ERR_NONE == pInputTool->Init(m_pRenderDevice,m_pOperMap,m_hWnd))
					{
						ushort unLineType = LT_Rect;

						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;
						param.wParam = WPARAM(&unLineType);
						param.lParam = NULL;

						pInputTool->Notify(GT_MSG_SET_INPUT_LINE_TYPE,param);

						this->BeginDelegate(pInputTool);
					}
				}
			}
			break;
		}
	}

	int SmtSelectTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		if(m_pDelegateTag != NULL && !m_pDelegateTag->IsOperDone())
			return m_pDelegateTag->KeyDown(nChar,nRepCnt,nFlags);
		else
		{
			if (!(GetKeyState( VK_CONTROL ) & 0x8000))
			{
				switch (nChar)
				{
				case 'c':
				case 'C':
					{
						m_pResultLayer->DeleteAll();

						SmtListenerMsg param;
						param.hSrcWnd = m_hWnd;
						param.wParam = WPARAM(m_pResultLayer);
						param.lParam = LPARAM(&m_nLayerFeaType);
						SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,SMT_IATOOL_MSG_KEY(GT_MSG_SET_FLASH_DATA),param);

						SetActive();
					}
					break;
				}
			}
		}

		return SmtBaseTool::KeyDown(nChar,nRepCnt,nFlags);
	}
}
