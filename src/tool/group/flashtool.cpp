#include <math.h>
#include <assert.h>

#include "tool/group/resource.h"
#include "tool/group/flashtool.h"
#include "base/core/api.h"
#include "base/style/stylemanager.h"
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "sys/sysmanager.h"
#include "sdb/feature/feature_api.h"
#include "base/style/style_api.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

#include <cstring>

using namespace render;
using namespace base;
using namespace sdb;
using namespace sdb;
using namespace sys;

const string						CST_STR_FLASH_TOOL_NAME	= "��˸";

namespace tool
{
	SmtFlashTool::SmtFlashTool()
		:m_flsMode(FM_2)
		,m_bFlash(false)
		,m_bStyle1(true)
		,m_fScaleDelt(0.15)
		,m_workspace(NULL)
	{
		set_name(CST_STR_FLASH_TOOL_NAME.c_str());
	}

	SmtFlashTool::~SmtFlashTool()
	{
		SmtDataSourceMgr::DestoryMemVecLayer(m_resultLayer);

		UnRegisterMsg();
	}

	int SmtFlashTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();

		SmtSysPra sysPra = pSysMgr->get_sys_pra();

		m_fScaleDelt = sysPra.fZoomScaleDelt;

		m_resultLayer = SmtDataSourceMgr::CreateMemVecLayer();

		append_func_items("��ʼ��˸",GT_MSG_START_FLASH,FIM_2DVIEW);
		append_func_items("ֹͣ��˸",GT_MSG_STOP_FLASH,FIM_2DVIEW);

		SMT_IATOOL_APPEND_MSG(GT_MSG_START_FLASH);
		SMT_IATOOL_APPEND_MSG(GT_MSG_STOP_FLASH);

		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_GET_FLASH_MODE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_PRA);
		SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_DATA);

		RegisterMsg();
	
		return SMT_ERR_NONE;
	}

	bool SmtFlashTool::session_flashing() const
	{
		if (m_workspace)
		{
			return m_workspace->flashing();
		}
		return m_bFlash;
	}

	int SmtFlashTool::AuxDraw()
	{
		if (session_flashing() && m_resultLayer.layer)
		{
			SmtStyleManager * pStyleMgr = SmtStyleManager::get_singleton_ptr();

			SmtStyle *pStyle = pStyleMgr->get_style(m_strFlashStyle.c_str());
			if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_DYNAMIC,true,pStyle,R2_COPYPEN))
			{
				m_pRenderDevice->RenderLayer(m_resultLayer.layer,R2_COPYPEN);
				m_pRenderDevice->EndRender(MRD_BL_DYNAMIC);
			}
		}
		
		return SMT_ERR_NONE;
	}

	int SmtFlashTool::Timer()
	{
		if (!session_flashing())
		{
			return SMT_ERR_NONE;
		}
		m_bStyle1 = !m_bStyle1;
		if (m_bStyle1)
			m_strFlashStyle = m_strFlashStyle1;
		else
			m_strFlashStyle = m_strFlashStyle2;

		return SMT_ERR_NONE;
	}

	int SmtFlashTool::notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		const char* cmd = tool::command_id_from_gt_msg(nMsg);
		if (cmd && std::strcmp(cmd, "flash.stop") == 0)
		{
			if (m_workspace)
			{
				tool::CommandArgs args;
				m_workspace->execute("flash.stop", args);
			}
			else
			{
				m_bFlash = false;
			}

			if (SMT_ERR_NONE == m_pRenderDevice->BeginRender(MRD_BL_DYNAMIC,true,NULL,R2_COPYPEN))
			{
				m_pRenderDevice->RenderLayer((SmtLayer*)NULL,R2_COPYPEN);
				m_pRenderDevice->EndRender(MRD_BL_DYNAMIC);
			}

			m_pRenderDevice->Refresh();
			return SMT_ERR_NONE;
		}
		if (cmd && std::strcmp(cmd, "flash.start") == 0)
		{
			if (m_workspace)
			{
				tool::CommandArgs args;
				m_workspace->execute("flash.start", args);
			}
			else
			{
				m_bFlash = true;
			}
			return SMT_ERR_NONE;
		}

		switch (nMsg)
		{
			case GT_MSG_DEFAULT_PROCESS:
				{

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
					*(ushort*)param.wParam = session_flashing() ? 1 : 0;
				}
				break;	
			case GT_MSG_SET_PRA:
				{
					//
				}
				break;
			case GT_MSG_SET_FLASH_DATA:
				{
					int nLayerFeaType = *(int*)param.lParam;
					auto* scratch = reinterpret_cast<ScratchLayer*>(param.wParam);
					OGRLayer* pSrcLayer = scratch ? scratch->layer : nullptr;
					SmtDataSourceMgr::DestoryMemVecLayer(m_resultLayer);
					m_resultLayer = SmtDataSourceMgr::CreateMemVecLayer();
					if (m_resultLayer.layer && pSrcLayer)
					{
						copy_layer(m_resultLayer.layer, pSrcLayer);
					}

					SmtSysManager *pSysMgr = SmtSysManager::get_singleton_ptr();
					// get_sys_style_config() returns by value; keep a local copy.
					const SmtStyleConfig style = pSysMgr->get_sys_style_config();

					switch (nLayerFeaType)
					{
					case SmtFeatureType::SmtFtChildImage:
					case SmtFeatureType::SmtFtDot:
					case SmtFeatureType::SmtFtAnno:
						{
							m_strFlashStyle1 = style.szDotFlashStyle1;
							m_strFlashStyle2 = style.szDotFlashStyle2; 
						}
						break;
					case SmtFeatureType::SmtFtCurve:
						{
							m_strFlashStyle1 = style.szLineFlashStyle1;
							m_strFlashStyle2 = style.szLineFlashStyle2; 
						}
						break;
					case SmtFeatureType::SmtFtSurface:
						{
							m_strFlashStyle1 = style.szRegionFlashStyle1;
							m_strFlashStyle2 = style.szRegionFlashStyle2; 
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
