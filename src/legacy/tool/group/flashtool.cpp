#include "legacy/tool/group/flashtool.h"

#include <assert.h>
#include <math.h>

#include <cstring>

#include "base/core/api.h"
#include "legacy/tool/group/resource.h"
#include "ogrsf_frmts.h"
#include "base/carto/style_api.h"
#include "base/carto/stylemanager.h"
#include "gis/datasource/mgr/datasource_mgr.h"
#include "gis/feature/feature.h"
#include "gis/feature/feature_api.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace gis;
using namespace sys;

const string CST_STR_FLASH_TOOL_NAME = "闪烁";

namespace tool {
SmtFlashTool::SmtFlashTool()
    : m_flsMode(FM_2),
      m_bFlash(false),
      m_bStyle1(true),
      m_fScaleDelt(0.15),
      m_workspace(NULL) {
  set_name(CST_STR_FLASH_TOOL_NAME.c_str());
}

SmtFlashTool::~SmtFlashTool() {
  DataSourceMgr::destroy_mem_vec_layer(m_resultLayer);

  UnRegisterMsg();
}

int SmtFlashTool::Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap,
                       HWND hWnd, pfnToolCallBack pfnCallBack,
                       void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  m_fScaleDelt = sysPra.fZoomScaleDelt;

  m_resultLayer = DataSourceMgr::create_mem_vec_layer();

  append_func_items("开始闪烁", GT_MSG_START_FLASH, FIM_2DVIEW);
  append_func_items("停止闪烁", GT_MSG_STOP_FLASH, FIM_2DVIEW);

  SMT_IATOOL_APPEND_MSG(GT_MSG_START_FLASH);
  SMT_IATOOL_APPEND_MSG(GT_MSG_STOP_FLASH);

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_FLASH_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_PRA);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_FLASH_DATA);

  RegisterMsg();

  return SMT_ERR_NONE;
}

bool SmtFlashTool::session_flashing() const {
  if (m_workspace) {
    return m_workspace->flashing();
  }
  return m_bFlash;
}

int SmtFlashTool::AuxDraw() {
  if (!session_flashing() || !m_resultLayer.layer || !m_pRenderDevice) {
    return SMT_ERR_NONE;
  }

  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();
  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyle* pStyle =
      pStyleMgr ? pStyleMgr->get_style(m_strFlashStyle.c_str()) : nullptr;
  if (!pStyle && pStyleMgr && pSysMgr) {
    const SmtStyleConfig style = pSysMgr->get_sys_style_config();
    pStyle = pStyleMgr->get_style(m_strFlashStyle1.c_str());
    if (!pStyle) {
      pStyle = pStyleMgr->get_style(style.szAuxStyle);
    }
  }

  // Paint AFTER Present on the window DC so MAP/QUICK blit cannot hide it.
  if (SMT_ERR_NONE ==
      m_pRenderDevice->BeginRender(MRD_BL_DIRECT, false, pStyle, R2_COPYPEN)) {
    m_pRenderDevice->RenderLayer(m_resultLayer.layer, R2_COPYPEN);
    m_pRenderDevice->EndRender(MRD_BL_DIRECT);
  }

  return SMT_ERR_NONE;
}

int SmtFlashTool::Timer() {
  if (!session_flashing()) {
    return SMT_ERR_NONE;
  }
  m_bStyle1 = !m_bStyle1;
  if (m_bStyle1)
    m_strFlashStyle = m_strFlashStyle1;
  else
    m_strFlashStyle = m_strFlashStyle2;

  return SMT_ERR_NONE;
}

int SmtFlashTool::notify(long nMsg, SmtListenerMsg& param) {
  if (param.hSrcWnd != m_hWnd) return SMT_ERR_NONE;

  const char* cmd = tool::command_id_from_gt_msg(nMsg);
  if (cmd && std::strcmp(cmd, "flash.stop") == 0) {
    if (!tool::try_execute_gt_msg(m_workspace, nMsg)) {
      m_bFlash = false;
    }

    if (SMT_ERR_NONE ==
        m_pRenderDevice->BeginRender(MRD_BL_DYNAMIC, true, NULL, R2_COPYPEN)) {
      m_pRenderDevice->RenderLayer((SmtLayer*)NULL, R2_COPYPEN);
      m_pRenderDevice->EndRender(MRD_BL_DYNAMIC);
    }

    m_pRenderDevice->Refresh();
    return SMT_ERR_NONE;
  }
  if (cmd && std::strcmp(cmd, "flash.start") == 0) {
    if (!tool::try_execute_gt_msg(m_workspace, nMsg)) {
      m_bFlash = true;
    }
    return SMT_ERR_NONE;
  }

  switch (nMsg) {
    case GT_MSG_DEFAULT_PROCESS: {
    } break;
    case GT_MSG_SET_FLASH_MODE: {
      m_flsMode = eFlashMode(*(ushort*)param.wParam);
    } break;
    case GT_MSG_GET_FLASH_MODE: {
      *(ushort*)param.wParam = m_flsMode;
    } break;
    case GT_MSG_GET_STATUS: {
      *(ushort*)param.wParam = session_flashing() ? 1 : 0;
    } break;
    case GT_MSG_SET_PRA: {
      //
    } break;
    case GT_MSG_SET_FLASH_DATA: {
      int nLayerFeaType = *(int*)param.lParam;
      auto* scratch = reinterpret_cast<ScratchLayer*>(param.wParam);
      OGRLayer* pSrcLayer = scratch ? scratch->layer : nullptr;
      DataSourceMgr::destroy_mem_vec_layer(m_resultLayer);
      m_resultLayer = DataSourceMgr::create_mem_vec_layer();
      if (m_resultLayer.layer && pSrcLayer) {
        copy_layer(m_resultLayer.layer, pSrcLayer);
      }

      SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
      // get_sys_style_config() returns by value; keep a local copy.
      const SmtStyleConfig style = pSysMgr->get_sys_style_config();

      // Mixed GeoJSON layers report Unknown/Tin; infer from first hit.
      if (nLayerFeaType == SmtFeatureType::SmtFtUnknown ||
          nLayerFeaType == SmtFeatureType::SmtFtTin ||
          nLayerFeaType == SmtFeatureType::SmtFtGrid) {
        if (m_resultLayer.layer) {
          m_resultLayer.layer->ResetReading();
          if (OGRFeature* feat = m_resultLayer.layer->GetNextFeature()) {
            if (OGRGeometry* g = feat->GetGeometryRef()) {
              switch (wkbFlatten(g->getGeometryType())) {
                case wkbPoint:
                case wkbMultiPoint:
                  nLayerFeaType = SmtFeatureType::SmtFtDot;
                  break;
                case wkbLineString:
                case wkbMultiLineString:
                  nLayerFeaType = SmtFeatureType::SmtFtCurve;
                  break;
                default:
                  nLayerFeaType = SmtFeatureType::SmtFtSurface;
                  break;
              }
            }
            OGRFeature::DestroyFeature(feat);
          }
        }
      }

      switch (nLayerFeaType) {
        case SmtFeatureType::SmtFtChildImage:
        case SmtFeatureType::SmtFtDot:
        case SmtFeatureType::SmtFtAnno: {
          m_strFlashStyle1 = style.szDotFlashStyle1;
          m_strFlashStyle2 = style.szDotFlashStyle2;
        } break;
        case SmtFeatureType::SmtFtCurve: {
          m_strFlashStyle1 = style.szLineFlashStyle1;
          m_strFlashStyle2 = style.szLineFlashStyle2;
        } break;
        case SmtFeatureType::SmtFtSurface:
        default: {
          m_strFlashStyle1 = style.szRegionFlashStyle1;
          m_strFlashStyle2 = style.szRegionFlashStyle2;
        } break;
      }
      m_strFlashStyle = m_strFlashStyle1;
    } break;
    default:
      break;
  }
  return SMT_ERR_NONE;
}
}  // namespace tool
