#include <math.h>
#include <assert.h>

#include "resource.h "
#include "gt_flashtool.h"
#include "smt_api.h"
#include "bl_stylemanager.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "sde_datasourcemgr.h"
#include "sys_sysmanager.h"
#include "gis_api.h"
#include "bl_api.h"

using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_SDEDevMgr;
using namespace Smt_Sys;

const string						CST_STR_FLASH_TOOL_NAME	= "ÉÁË¸";

namespace Smt_GroupTool
{
	SmtFlashTool::SmtFlashTool()
		:m_bStyle1(true)
		,m_pResultLayer(NULL)
		,m_fScaleDelt(0.15)
	{
		SetName(CST_STR_FLASH_TOOL_NAME.c_str());
	}

	SmtFlashTool::~SmtFlashTool()
	{
		SmtDataSourceMgr::DestoryMemVecLayer(m_pResultLayer);

		UnRegisterMsg();
	}

	int SmtFlashTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra = pSysMgr->GetSysPra();

		m_fScaleDelt = sysPra.fZoomScaleDelt;

		m_pResultLayer = SmtDataSourceMgr::CreateMemVecLayer();
		m_pResultLayer->Open("");

		AppendFuncItems("¿ªÊ¼ÉÁË¸",GT_MSG_START_FLASH,FIM_2DVIEW);
		AppendFuncItems("Í£Ö¹ÉÁË¸",GT_MSG_STOP_FLASH,FIM_2DVIEW);

		SMT_IATOOL_APPEND_MSG(GT_MSG_START_FLASH);
		SMT_IATOOL_APPEND_MSG(GT_MSG_STOP_FLASH);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_FLASH_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_PRA);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_DATA);

		RegisterMsg();
	
		return SMT_ERR_NONE;
	}

	int SmtFlashTool::AuxDraw()
	{
		if (m_bFlash)
		{
			SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();

			SmtStyle *pStyle = pStyleMgr->GetStyle(m_strFlashStyle.c_str());
			if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_DYNAMIC,true,pStyle,R2_COPYPEN))
			{
				m_pRenderDevice->RenderLayer(m_pResultLayer,R2_COPYPEN);
				m_pRenderDevice->EndRender(MRD_BL_DYNAMIC);
			}
		}
		
		return SMT_ERR_NONE;
	}

	int SmtFlashTool::Timer()
	{
		m_bStyle1 = !m_bStyle1;
		if (m_bStyle1)
			m_strFlashStyle = m_strFlashStyle1;
		else
			m_strFlashStyle = m_strFlashStyle2;

		return SMT_ERR_NONE;
	}

	int SmtFlashTool::Notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		switch (nMsg)
		{
			case GT_MSG_DEFAULT_PROCESS:
				{

				}
				break;
			case GT_MSG_STOP_FLASH:
				{
					m_bFlash = false;

					if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_DYNAMIC,true,NULL,R2_COPYPEN))
					{
						m_pRenderDevice->RenderLayer((SmtLayer*)NULL,R2_COPYPEN);
						m_pRenderDevice->EndRender(MRD_BL_DYNAMIC);
					}
	
					m_pRenderDevice->Refresh();
				}
				break;
			case GT_MSG_START_FLASH:
				{
					m_bFlash = true;
				}
				break;
			case GT_MSG_SET_FLASH_MODE:
				{
					m_flsMode = eFlashMode(*(ushort*)param.wParam);
				}
				break;
			case GT_MSG_GET_FLASH_MODE:
				{
					*(ushort*)param.wParam = m_flsMode;
				}
				break;
			case GT_MSG_GET_STATUS:
				{
					*(ushort*)param.wParam = (m_bFlash)?1:0;
				}
				break;	
			case GT_MSG_SET_PRA:
				{
					//
				}
				break;
			case GT_MSG_SET_FLASH_DATA:
				{
					//
					int nLayerFeaType = *(int*)param.lParam;
					SmtVectorLayer *pSrcLayer = (SmtVectorLayer *)param.wParam;
					m_pResultLayer->DeleteAll();
					CopyLayer(m_pResultLayer,pSrcLayer);

					SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

					switch (nLayerFeaType)
					{
					case SmtFeatureType::SmtFtChildImage:
					case SmtFeatureType::SmtFtDot:
					case SmtFeatureType::SmtFtAnno:
						{
							m_strFlashStyle1 = pSysMgr->GetSysStyleConfig().szDotFlashStyle1;
							m_strFlashStyle2 = pSysMgr->GetSysStyleConfig().szDotFlashStyle2; 
						}
						break;
					case SmtFeatureType::SmtFtCurve:
						{
							m_strFlashStyle1 = pSysMgr->GetSysStyleConfig().szLineFlashStyle1;
							m_strFlashStyle2 = pSysMgr->GetSysStyleConfig().szLineFlashStyle2; 
						}
						break;
					case SmtFeatureType::SmtFtSurface:
						{
							m_strFlashStyle1 = pSysMgr->GetSysStyleConfig().szRegionFlashStyle1;
							m_strFlashStyle2 = pSysMgr->GetSysStyleConfig().szRegionFlashStyle2; 
						}
						break;
					}
				}
				break;
			default:
				break;
		}
		return SMT_ERR_NONE;
	}
}
