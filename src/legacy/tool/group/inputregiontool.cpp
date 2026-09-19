#include "legacy/tool/group/inputregiontool.h"

#include <cstdint>

#include "algorithm/geo/geometry.h"
#include "base/carto/style_api.h"
#include "base/carto/stylemanager.h"
#include "base/core/api.h"
#include "legacy/tool/group/defs.h"
#include "sys/sysmanager.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace geo;
using namespace sys;

const string CST_STR_INPUTREGION_TOOL_NAME = "???";

namespace tool {
namespace {
void add_draft_points(LPRENDERDEVICE device, const tool::Draft& draft,
                      OGRLineString* geom) {
  for (const tool::DraftPoint& p : draft.points) {
    float x = 0;
    float y = 0;
    device->DPToLP(p.x_px, p.y_px, x, y);
    geom->addPoint(x, y);
  }
}

void add_rect_ring(LPRENDERDEVICE device, const tool::Draft& draft,
                   OGRLineString* geom) {
  if (draft.points.size() < 2) {
    add_draft_points(device, draft, geom);
    return;
  }
  const int32_t x0 = draft.points[0].x_px;
  const int32_t y0 = draft.points[0].y_px;
  const int32_t x1 = draft.points[1].x_px;
  const int32_t y1 = draft.points[1].y_px;
  const tool::DraftPoint corners[] = {
      {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
  };
  for (const tool::DraftPoint& p : corners) {
    float x = 0;
    float y = 0;
    device->DPToLP(p.x_px, p.y_px, x, y);
    geom->addPoint(x, y);
  }
}
}  // namespace

SmtInputRegionTool::SmtInputRegionTool()
    : m_pGeom(NULL), m_appendType(RT_Polygon) {
  set_name(CST_STR_INPUTREGION_TOOL_NAME.c_str());
}

SmtInputRegionTool::~SmtInputRegionTool() {
  SMT_SAFE_DELETE(m_pGeom);

  UnRegisterMsg();
}

int SmtInputRegionTool::Init(LPRENDERDEVICE pMrdRenderDevice,
                             SmtMap* pOperSmtMap, HWND hWnd,
                             pfnToolCallBack pfnCallBack, void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();
  SmtStyle* pStyle = pStyleMgr->get_style(m_szStyleName);
  if (pStyle) {
    pStyle->set_style_type(ST_PenDesc | ST_BrushDesc);
  }

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_INPUT_REGION_TYPE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_INPUT_REGION_TYPE);

  RegisterMsg();
  OnSetRegionType();

  return SMT_ERR_NONE;
}

int SmtInputRegionTool::AuxDraw() { return SMT_ERR_NONE; }

int SmtInputRegionTool::notify(long nMsg, SmtListenerMsg& param) {
  if (param.hSrcWnd != m_hWnd) return SMT_ERR_NONE;

  switch (nMsg) {
    case GT_MSG_SET_INPUT_REGION_TYPE: {
      m_appendType = *(ushort*)param.wParam;
      OnSetRegionType();
    } break;
    case GT_MSG_GET_INPUT_REGION_TYPE: {
      *(ushort*)param.wParam = m_appendType;
    } break;
    default:
      break;
  }
  return SMT_ERR_NONE;
}

void SmtInputRegionTool::OnSetRegionType(void) {
  // Type only - callers SetActive when they need exclusive IA.
}

void SmtInputRegionTool::apply_draft(const tool::Draft& draft) {
  if (!m_pRenderDevice || draft.points.empty()) {
    return;
  }
  OGRLinearRing* ring = new OGRLinearRing();
  if (draft.kind == tool::DraftKind::kRect) {
    add_rect_ring(m_pRenderDevice, draft, ring);
  } else {
    add_draft_points(m_pRenderDevice, draft, ring);
  }
  ring->closeRings();
  OGRPolygon* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  SMT_SAFE_DELETE(m_pGeom);
  m_pGeom = poly;
  EndAppendRegion();
}

void SmtInputRegionTool::EndAppendRegion() {
  if (m_pRenderDevice) {
    m_pRenderDevice->Refresh();
  }

  ushort uRetType = GT_MSG_RET_INPUT_REGION;
  SmtListenerMsg param;

  param.hSrcWnd = m_hWnd;
  param.wParam = WPARAM(m_pGeom);
  param.lParam = LPARAM(&uRetType);

  EndIA(GT_MSG_RET_DELEGATE, param);

  SMT_SAFE_DELETE(m_pGeom);
  SetOperDone(true);
}
}  // namespace tool
