#include "legacy/tool/select/selecttool.h"

#include "base/core/log.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/msg/msg.h"
#include "legacy/tool/select/select_query_apply.h"
#include "tool/workspace/workspace.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace sys;

const string CST_STR_SELECT_TOOL_NAME = "选取";

namespace tool {
SmtSelectTool::SmtSelectTool()
    : m_selMode(ST_Point), m_nLayerFeaType(FtUnknown), m_dpMargin(4) {
  set_name(CST_STR_SELECT_TOOL_NAME.c_str());
}

SmtSelectTool::~SmtSelectTool() {
  SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

  this->EndDelegate();

  gis::DataSourceMgr::destroy_mem_vec_layer(m_resultLayer);

  UnRegisterMsg();
}

int SmtSelectTool::Init(LPRENDERDEVICE pMrdRenderDevice, Map* pOperSmtMap,
                        HWND hWnd, pfnToolCallBack pfnCallBack,
                        void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  m_resultLayer = gis::DataSourceMgr::create_mem_vec_layer();

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();

  SmtSysPra sysPra = pSysMgr->get_sys_pra();

  m_dpMargin = sysPra.fSmargin;

  append_func_items("Point Select", GT_MSG_SELECT_POINTSEL,
                    FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("Rect Select", GT_MSG_SELECT_RECTSEL,
                    FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("Polygon Select", GT_MSG_SELECT_POLYGONSEL,
                    FIM_2DVIEW | FIM_2DMFMENU);
  append_func_items("Clear Selection", GT_MSG_SELECT_CLEAR,
                    FIM_2DVIEW | FIM_2DMFMENU);

  SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POINTSEL);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_RECTSEL);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_POLYGONSEL);
  SMT_IATOOL_APPEND_MSG(GT_MSG_SELECT_CLEAR);

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_SEL_MODE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_SEL_MODE);

  RegisterMsg();

  return SMT_ERR_NONE;
}

int SmtSelectTool::AuxDraw() { return SmtBaseTool::AuxDraw(); }

int SmtSelectTool::Timer() { return SmtBaseTool::AuxDraw(); }

int SmtSelectTool::LButtonDown(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonDown(nFlags, point);
}

int SmtSelectTool::LButtonUp(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonUp(nFlags, point);
}

int SmtSelectTool::MouseMove(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::MouseMove(nFlags, point);
}

int SmtSelectTool::notify(long nMsg, SmtListenerMsg& param) {
  if (param.hSrcWnd != m_hWnd) return SMT_ERR_NONE;

  switch (nMsg) {
    case GT_MSG_DEFAULT_PROCESS: {
    } break;
    case GT_MSG_SELECT_POINTSEL: {
      tool::try_execute_gt_msg(m_workspace, nMsg);
      m_selMode = ST_Point;
      if (m_workspace) {
        m_workspace->set_draft_flags(0);
      }

      OnSetSelMode();
      param.bModify = true;

      if (!m_workspace) {
        SetActive();
      }
    } break;
    case GT_MSG_SELECT_RECTSEL: {
      tool::try_execute_gt_msg(m_workspace, nMsg);
      m_selMode = ST_Rect;
      if (m_workspace) {
        m_workspace->set_draft_flags(0);
      }

      OnSetSelMode();
      param.bModify = true;

      if (!m_workspace) {
        SetActive();
      }
    } break;
    case GT_MSG_SELECT_POLYGONSEL: {
      tool::try_execute_gt_msg(m_workspace, nMsg);
      m_selMode = ST_Polygon;
      if (m_workspace) {
        m_workspace->set_draft_flags(0);
      }

      OnSetSelMode();
      param.bModify = true;

      if (!m_workspace) {
        SetActive();
      }
    } break;
    case GT_MSG_SELECT_CLEAR: {
      tool::try_execute_gt_msg(m_workspace, nMsg);
      clear_select_scratch(m_resultLayer);
      post_select_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

      if (!m_workspace) {
        SetActive();
      }
    } break;

    case GT_MSG_SET_SEL_MODE: {
      m_selMode = eSelectMode(*(ushort*)param.wParam);

      OnSetSelMode();

      if (m_workspace) {
        switch (m_selMode) {
          case ST_Point:
            m_workspace->set_draft_flags(0);
            m_workspace->activate("select.point");
            break;
          case ST_Rect:
            m_workspace->set_draft_flags(0);
            m_workspace->activate("select.rect");
            break;
          case ST_Circle:
            m_workspace->set_draft_flags(
                tool::draft_flags::pack(tool::draft_flags::kFamilySelect,
                                        tool::draft_flags::kSelectCircleCode));
            m_workspace->activate("select.circle");
            break;
          case ST_Polygon:
            m_workspace->set_draft_flags(0);
            m_workspace->activate("select.polygon");
            break;
        }
      }

      param.bModify = true;
    } break;
    case GT_MSG_GET_SEL_MODE: {
      *(ushort*)param.wParam = m_selMode;
    } break;
    default:
      break;
  }
  return SMT_ERR_NONE;
}

void SmtSelectTool::OnRetDelegate(int nRetType) {
  const bool point_query = (nRetType == GT_MSG_RET_INPUT_POINT);
  run_select_query(m_pRenderDevice, m_pOperMap, m_resultLayer, m_gQDes, m_pQDes,
                   m_nLayerFeaType, m_dpMargin, m_hWnd, point_query);
}

void SmtSelectTool::OnSetSelMode(void) { this->EndDelegate(); }

void SmtSelectTool::apply_draft(const tool::Draft& draft) {
  SMT_SAFE_DELETE(m_gQDes.pQueryGeom);
  m_gQDes.pQueryGeom =
      query_geom_from_select_draft(m_pRenderDevice, draft, m_selMode == ST_Circle);
  if (!m_gQDes.pQueryGeom) {
    return;
  }
  const bool point_query = (draft.kind == tool::DraftKind::kPoint);
  OnRetDelegate(point_query ? GT_MSG_RET_INPUT_POINT : GT_MSG_RET_INPUT_LINE);
}

int SmtSelectTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags) {
  if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
    switch (nChar) {
      case 'c':
      case 'C': {
        clear_select_scratch(m_resultLayer);
        post_select_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

        if (!m_workspace) {
          SetActive();
        }
      } break;
    }
  }

  return SmtBaseTool::KeyDown(nChar, nRepCnt, nFlags);
}
}  // namespace tool
