#include "legacy/tool/group/inputlinetool.h"

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

const string CST_STR_INPUTLINE_TOOL_NAME = "???";

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

SmtInputLineTool::SmtInputLineTool()
    : m_pGeom(NULL), m_appendType(LT_LineString) {
  set_name(CST_STR_INPUTLINE_TOOL_NAME.c_str());
}

SmtInputLineTool::~SmtInputLineTool() {
  SMT_SAFE_DELETE(m_pGeom);

  UnRegisterMsg();
}

int SmtInputLineTool::Init(LPRENDERDEVICE pMrdRenderDevice, SmtMap* pOperSmtMap,
                           HWND hWnd, pfnToolCallBack pfnCallBack,
                           void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();
  SmtStyle* pStyle = pStyleMgr->get_style(m_szStyleName);
  if (pStyle) {
    pStyle->set_style_type(ST_PenDesc);
  }

  SMT_IATOOL_APPEND_MSG(GT_MSG_SET_INPUT_LINE_TYPE);
  SMT_IATOOL_APPEND_MSG(GT_MSG_GET_INPUT_LINE_TYPE);

  RegisterMsg();
  OnSetLineType();

  return SMT_ERR_NONE;
}

int SmtInputLineTool::AuxDraw() { return SMT_ERR_NONE; }

int SmtInputLineTool::notify(long nMsg, SmtListenerMsg& param) {
  if (param.hSrcWnd != m_hWnd) return SMT_ERR_NONE;

  switch (nMsg) {
    case GT_MSG_SET_INPUT_LINE_TYPE: {
      m_appendType = *(ushort*)param.wParam;
      OnSetLineType();
    } break;
    case GT_MSG_GET_INPUT_LINE_TYPE: {
      *(ushort*)param.wParam = m_appendType;
    } break;
    default:
      break;
  }
  return SMT_ERR_NONE;
}

void SmtInputLineTool::OnSetLineType(void) {
  // Type only - callers (e.g. orthogrid) SetActive when they need exclusive IA.
}

void SmtInputLineTool::apply_draft(const tool::Draft& draft) {
  if (!m_pRenderDevice || draft.points.empty()) {
    return;
  }
  SMT_SAFE_DELETE(m_pGeom);
  if (m_appendType == LT_LinearRing ||
      draft.kind == tool::DraftKind::kPolygon) {
    OGRLinearRing* ring = new OGRLinearRing();
    if (draft.kind == tool::DraftKind::kRect) {
      add_rect_ring(m_pRenderDevice, draft, ring);
    } else {
      add_draft_points(m_pRenderDevice, draft, ring);
    }
    ring->closeRings();
    m_pGeom = ring;
  } else {
    OGRLineString* line = new OGRLineString();
    if (draft.kind == tool::DraftKind::kRect || m_appendType == LT_Rect) {
      add_rect_ring(m_pRenderDevice, draft, line);
    } else {
      add_draft_points(m_pRenderDevice, draft, line);
    }
    m_pGeom = line;
  }
  EndAppendLine();
}

void SmtInputLineTool::EndAppendLine(void) {
  if (m_pRenderDevice) {
    m_pRenderDevice->Refresh();
  }

  ushort uRetType = GT_MSG_RET_INPUT_LINE;
  SmtListenerMsg param;

  param.hSrcWnd = m_hWnd;
  param.wParam = WPARAM(m_pGeom);
  param.lParam = LPARAM(&uRetType);

  EndIA(GT_MSG_RET_DELEGATE, param);

  SMT_SAFE_DELETE(m_pGeom);

  SetOperDone(true);
}
}  // namespace tool
