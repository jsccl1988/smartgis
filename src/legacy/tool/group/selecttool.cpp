#include "legacy/tool/group/selecttool.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "algorithm/geo/geometry.h"
#include "base/core/api.h"
#include "base/core/log.h"
#include "base/core/msg_def.h"
#include "legacy/tool/t_iatoolmanager.h"
#include "legacy/ui/gui/gui_api.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/ogr_feature_codec.h"
#include "gis/datasource/mgr/datasource_mgr.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace geo;
using namespace base;
using namespace sys;
using namespace gis;

const string CST_STR_SELECT_TOOL_NAME = "选取";
const string C_STR_SELECT_TOO_LOG = "SmtSelectTool";

namespace {

void clear_scratch(ScratchLayer& sl) {
  DataSourceMgr::destroy_mem_vec_layer(sl);
  sl = DataSourceMgr::create_mem_vec_layer();
}

int ogr_feature_count(OGRLayer* layer) {
  if (!layer) {
    return 0;
  }
  const int n = static_cast<int>(layer->GetFeatureCount());
  return n < 0 ? 0 : n;
}

void collect_fids(OGRLayer* layer, std::vector<uint>& ids) {
  if (!layer) {
    return;
  }
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    ids.push_back(static_cast<uint>(feat->GetFID()));
    OGRFeature::DestroyFeature(feat);
  }
}

void post_flash_data(HWND hwnd, ScratchLayer* scratch, int* fea_type) {
  SmtListenerMsg param;
  param.hSrcWnd = hwnd;
  param.wParam = WPARAM(scratch);
  param.lParam = LPARAM(fea_type);
  post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                   SMT_MSG_KEY(GT_MSG_SET_FLASH_DATA, hwnd), param);
}

void post_flash_start(HWND hwnd) {
  SmtListenerMsg param;
  param.hSrcWnd = hwnd;
  post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                   SMT_MSG_KEY(GT_MSG_START_FLASH, hwnd), param);
}

void refresh_fea_type(SmtMap* map, int& fea_type) {
  if (!map) {
    return;
  }
  fea_type = gis::datasource::feature_type_of(map->GetActiveOgrLayer());
}

float query_margin_lp(LPRENDERDEVICE device, double dp_margin) {
  if (!device) {
    return 0.05f;
  }
  const double blc = device->GetBlc();
  if (blc <= 1e-12) {
    return 0.05f;
  }
  return static_cast<float>(dp_margin / blc);
}

}  // namespace

namespace tool {
SmtSelectTool::SmtSelectTool()
    : m_selMode(ST_Point), m_nLayerFeaType(SmtFtUnknown), m_dpMargin(4) {
  set_name(CST_STR_SELECT_TOOL_NAME.c_str());
}

SmtSelectTool::~SmtSelectTool() {
  SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

  this->EndDelegate();

  DataSourceMgr::destroy_mem_vec_layer(m_resultLayer);

  UnRegisterMsg();
}

int SmtSelectTool::Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap,
                        HWND hWnd, pfnToolCallBack pfnCallBack,
                        void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  m_resultLayer = DataSourceMgr::create_mem_vec_layer();

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
      clear_scratch(m_resultLayer);
      post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

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
            m_workspace->set_draft_flags(tool::draft_flags::pack(
                tool::draft_flags::kFamilySelect,
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
  switch (nRetType) {
    case GT_MSG_RET_INPUT_POINT: {
      if (!(GetAsyncKeyState(VK_LCONTROL) & 0x8000))
        clear_scratch(m_resultLayer);

      m_gQDes.fSmargin = query_margin_lp(m_pRenderDevice, m_dpMargin);
      if (m_resultLayer.layer) {
        m_pOperMap->QueryFeature(&m_gQDes, &m_pQDes, m_resultLayer.layer,
                                 m_nLayerFeaType);
      }
      if (m_nLayerFeaType == SmtFtUnknown) {
        refresh_fea_type(m_pOperMap, m_nLayerFeaType);
      }

      SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

      post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);
      post_flash_start(m_hWnd);
    } break;
    case GT_MSG_RET_INPUT_LINE: {
      if (!(GetAsyncKeyState(VK_LCONTROL) & 0x8000))
        clear_scratch(m_resultLayer);

      m_gQDes.fSmargin = query_margin_lp(m_pRenderDevice, m_dpMargin);
      if (m_resultLayer.layer) {
        m_pOperMap->QueryFeature(&m_gQDes, &m_pQDes, m_resultLayer.layer,
                                 m_nLayerFeaType);
      }
      if (m_nLayerFeaType == SmtFtUnknown) {
        refresh_fea_type(m_pOperMap, m_nLayerFeaType);
      }

      SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

      post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);
      post_flash_start(m_hWnd);

      const int count = ogr_feature_count(m_resultLayer.layer);
      if (count > 1) {
        uint unID = SMT_C_INVALID_UINT_VALUE;
        std::vector<uint> vIDs;
        collect_fids(m_resultLayer.layer, vIDs);
        SmtSelectOneDlg(unID, vIDs);
      }
    } break;
  }
}

void SmtSelectTool::OnSetSelMode(void) { this->EndDelegate(); }

void SmtSelectTool::apply_draft(const tool::Draft& draft) {
  if (!m_pRenderDevice || draft.points.empty()) {
    return;
  }

  SMT_SAFE_DELETE(m_gQDes.pQueryGeom);

  auto to_lp = [this](const tool::DraftPoint& p, float& x, float& y) {
    m_pRenderDevice->DPToLP(p.x_px, p.y_px, x, y);
  };

  if (draft.kind == tool::DraftKind::kPoint) {
    float x = 0;
    float y = 0;
    to_lp(draft.points[0], x, y);
    m_gQDes.pQueryGeom = new OGRPoint(x, y);
    OnRetDelegate(GT_MSG_RET_INPUT_POINT);
    return;
  }

  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    to_lp(draft.points[0], x0, y0);
    to_lp(draft.points[1], x1, y1);

    const bool circle = tool::draft_flags::is_select_circle(draft.flags) ||
                        m_selMode == ST_Circle;
    if (circle) {
      const float cx = (x0 + x1) * 0.5f;
      const float cy = (y0 + y1) * 0.5f;
      const float dx = x1 - x0;
      const float dy = y1 - y0;
      const float r = std::sqrt(dx * dx + dy * dy) * 0.5f;
      OGRLinearRing* ring = new OGRLinearRing();
      constexpr int kSegs = 32;
      for (int i = 0; i < kSegs; ++i) {
        const float ang =
            static_cast<float>(i) * 6.28318530718f / static_cast<float>(kSegs);
        ring->addPoint(cx + r * std::cos(ang), cy + r * std::sin(ang));
      }
      ring->closeRings();
      OGRPolygon* poly = new OGRPolygon();
      poly->addRingDirectly(ring);
      m_gQDes.pQueryGeom = poly;
      OnRetDelegate(GT_MSG_RET_INPUT_LINE);
      return;
    }

    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(x0, y0);
    ring->addPoint(x1, y0);
    ring->addPoint(x1, y1);
    ring->addPoint(x0, y1);
    ring->closeRings();
    OGRPolygon* poly = new OGRPolygon();
    poly->addRingDirectly(ring);
    m_gQDes.pQueryGeom = poly;
    OnRetDelegate(GT_MSG_RET_INPUT_LINE);
    return;
  }

  OGRLinearRing* ring = new OGRLinearRing();
  for (const tool::DraftPoint& p : draft.points) {
    float x = 0;
    float y = 0;
    to_lp(p, x, y);
    ring->addPoint(x, y);
  }
  ring->closeRings();
  m_gQDes.pQueryGeom = ring;
  OnRetDelegate(GT_MSG_RET_INPUT_LINE);
}

int SmtSelectTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags) {
  if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
    switch (nChar) {
      case 'c':
      case 'C': {
        clear_scratch(m_resultLayer);
        post_flash_data(m_hWnd, &m_resultLayer, &m_nLayerFeaType);

        if (!m_workspace) {
          SetActive();
        }
      } break;
    }
  }

  return SmtBaseTool::KeyDown(nChar, nRepCnt, nFlags);
}
}  // namespace tool
