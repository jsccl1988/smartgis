#include "legacy/tool/group/appendfeaturetool.h"

#include <math.h>

#include <cstdint>
#include <memory>

#include "algorithm/geo/geometry.h"
#include "base/core/api.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/group/resource.h"
#include "legacy/ui/gui/gui_api.h"
#include "ogrsf_frmts.h"
#include "base/carto/style_api.h"
#include "base/carto/stylemanager.h"
#include "gis/datasource/gdal/ogr_feature_codec.h"
#include "gis/feature/feature.h"
#include "gis/map/map.h"
#include "sys/sysmanager.h"
#include "tool/gestures.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

using namespace render;
using namespace base;
using namespace gis;
using namespace geo;
using namespace sys;
using namespace base;

const string CST_STR_APPENDFEATURE_TOOL_NAME = "添加要素";

namespace {

OGRFeature* make_ogr_feature(SmtMap* map, OGRGeometry* geom, SmtFeatureType ft,
                             const char* style_name) {
  if (!map || !geom) {
    return nullptr;
  }
  OGRLayer* lyr = map->GetActiveOgrLayer();
  if (!lyr) {
    return nullptr;
  }
  OGRFeature* ogr = OGRFeature::CreateFeature(lyr->GetLayerDefn());
  if (!ogr) {
    return nullptr;
  }
  if (!gis::datasource::encode_smt_geometry(geom, ogr, ft)) {
    OGRFeature::DestroyFeature(ogr);
    return nullptr;
  }
  if (style_name && style_name[0]) {
    SmtStyleManager* mgr = SmtStyleManager::get_singleton_ptr();
    if (mgr) {
      gis::datasource::copy_smt_style_to_ogr(mgr->get_style(style_name), ogr);
    }
  }
  return ogr;
}

bool commit_append(gis::MapEditSession* edits, OGRFeature* ogr) {
  if (!edits || !ogr) {
    if (ogr) {
      OGRFeature::DestroyFeature(ogr);
    }
    return false;
  }
  if (!edits->commit_feature(gis::EditOp::kAppend, ogr)) {
    OGRFeature::DestroyFeature(ogr);
    return false;
  }
  return true;
}

void set_field_string(OGRFeature* ogr, const char* name, const char* value) {
  if (!ogr || !name) {
    return;
  }
  const int i = ogr->GetFieldIndex(name);
  if (i >= 0) {
    ogr->SetField(i, value);
  }
}

void set_field_int(OGRFeature* ogr, const char* name, int value) {
  if (!ogr || !name) {
    return;
  }
  const int i = ogr->GetFieldIndex(name);
  if (i >= 0) {
    ogr->SetField(i, value);
  }
}

void set_field_double(OGRFeature* ogr, const char* name, double value) {
  if (!ogr || !name) {
    return;
  }
  const int i = ogr->GetFieldIndex(name);
  if (i >= 0) {
    ogr->SetField(i, value);
  }
}

}  // namespace

namespace tool {
SmtAppendFeatureTool::SmtAppendFeatureTool()
    : m_pGeom(NULL),
      m_strAnno("Smart GIS"),
      m_pointType(PT_DOT),
      m_lineType(LT_LineString),
      m_regionType(RT_Polygon),
      m_digitizeKind(0) {
  set_name(CST_STR_APPENDFEATURE_TOOL_NAME.c_str());
}

SmtAppendFeatureTool::~SmtAppendFeatureTool() {
  this->EndDelegate();
  m_pGeom = NULL;

  UnRegisterMsg();
}

int SmtAppendFeatureTool::Init(LPRENDERDEVICE pMrdRenderDevice,
                               SmtMap* pOperSmtMap, HWND hWnd,
                               pfnToolCallBack pfnCallBack, void* pToFollow) {
  if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice, pOperSmtMap, hWnd,
                                        pfnCallBack, pToFollow)) {
    return SMT_ERR_FAILURE;
  }

  m_edits = std::make_unique<gis::MapEditSession>(pOperSmtMap);

  SmtStyleManager* pStyleMgr = SmtStyleManager::get_singleton_ptr();
  SmtStyle* pStyle = pStyleMgr->get_style(m_szStyleName);
  pStyle->set_style_type(ST_PenDesc | ST_BrushDesc | ST_SymbolDesc |
                         ST_AnnoDesc);

  //
  append_func_items("添加子图", GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加注记", GT_MSG_APPEND_POINT_ANNO_FEATURE, FIM_2DMFMENU);
  append_func_items("添加点", GT_MSG_APPEND_POINT_DOT_FEATURE, FIM_2DMFMENU);

  //
  append_func_items("添加折线", GT_MSG_APPEND_LINE_LINESTRING_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加拉格朗日样条", GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加贝塞尔样条", GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加B样条", GT_MSG_APPEND_LINE_SPLINE_B_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加三次样条", GT_MSG_APPEND_LINE_SPLINE_3_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加矩形", GT_MSG_APPEND_LINE_RECT_FEATURE, FIM_2DMFMENU);
  append_func_items("添加圆弧", GT_MSG_APPEND_LINE_ARC_FEATURE, FIM_2DMFMENU);
  append_func_items("添加闭合线", GT_MSG_APPEND_LINE_LINEARRING_FEATURE,
                    FIM_2DMFMENU);

  //
  append_func_items("添加扇形", GT_MSG_APPEND_SURF_FAN_FEATURE, FIM_2DMFMENU);
  append_func_items("添加矩形面", GT_MSG_APPEND_SURF_RECT_FEATURE,
                    FIM_2DMFMENU);
  append_func_items("添加多边形", GT_MSG_APPEND_SURF_POLYGON_FEATURE,
                    FIM_2DMFMENU);

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

int SmtAppendFeatureTool::AuxDraw() { return SmtBaseTool::AuxDraw(); }

int SmtAppendFeatureTool::Timer() { return SmtBaseTool::Timer(); }

void SmtAppendFeatureTool::SetOperMap(SmtMap* pOperSmtMap) {
  SmtBaseTool::SetOperMap(pOperSmtMap);

  m_edits = std::make_unique<gis::MapEditSession>(pOperSmtMap);
}

int SmtAppendFeatureTool::LButtonDown(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonDown(nFlags, point);
}

int SmtAppendFeatureTool::LButtonUp(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::LButtonUp(nFlags, point);
}

int SmtAppendFeatureTool::MouseMove(uint nFlags, lPoint point) {
  if (m_workspace) {
    (void)nFlags;
    (void)point;
    return SMT_ERR_NONE;
  }
  return SmtBaseTool::MouseMove(nFlags, point);
}

int SmtAppendFeatureTool::notify(long nMsg, SmtListenerMsg& param) {
  if (param.hSrcWnd != m_hWnd) return SMT_ERR_NONE;

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

  // When Workspace is bound, activate draw.* and keep digitize kind; skip
  // leftover SetActive so exclusive input stays on Workspace.
  const bool via_ws = tool::try_execute_gt_msg(m_workspace, nMsg);

  switch (nMsg) {
    case GT_MSG_DEFAULT_PROCESS: {
    } break;
    case GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE: {
      ushort unType = PT_ChildImage;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 1;
        m_pointType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyPoint, unType));
      } else {
        OnInputPointFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_POINT_ANNO_FEATURE: {
      ushort unType = PT_Text;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 1;
        m_pointType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyPoint, unType));
      } else {
        OnInputPointFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_POINT_DOT_FEATURE: {
      ushort unType = PT_DOT;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 1;
        m_pointType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyPoint, unType));
      } else {
        OnInputPointFeature(unType);
      }
      param.bModify = true;
    } break;
    //////////////////////////////////////////////////////////////////////////
    case GT_MSG_APPEND_LINE_RECT_FEATURE: {
      ushort unType = LT_Rect;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_ARC_FEATURE: {
      ushort unType = LT_Arc;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_LINEARRING_FEATURE: {
      ushort unType = LT_LinearRing;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_LINESTRING_FEATURE: {
      ushort unType = LT_LineString;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE: {
      ushort unType = LT_Spline_Lag;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE: {
      ushort unType = LT_Spline_Bzer;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_SPLINE_B_FEATURE: {
      ushort unType = LT_Spline_B;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_LINE_SPLINE_3_FEATURE: {
      ushort unType = LT_Spline_3;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 2;
        m_lineType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyLine, unType));
      } else {
        OnInputLineFeature(unType);
      }
      param.bModify = true;
    } break;
    //////////////////////////////////////////////////////////////////////////
    case GT_MSG_APPEND_SURF_FAN_FEATURE: {
      ushort unType = RT_Fan;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 3;
        m_regionType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyRegion, unType));
      } else {
        OnInputRegionFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_SURF_RECT_FEATURE: {
      ushort unType = RT_Rect;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 3;
        m_regionType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyRegion, unType));
      } else {
        OnInputRegionFeature(unType);
      }
      param.bModify = true;
    } break;
    case GT_MSG_APPEND_SURF_POLYGON_FEATURE: {
      ushort unType = RT_Polygon;
      if (via_ws) {
        this->EndDelegate();
        m_digitizeKind = 3;
        m_regionType = unType;
        m_workspace->set_draft_flags(tool::draft_flags::pack(
            tool::draft_flags::kFamilyRegion, unType));
      } else {
        OnInputRegionFeature(unType);
      }
      param.bModify = true;
    } break;
    //////////////////////////////////////////////////////////////////////////
    default:
      break;
  }
  return SMT_ERR_NONE;
}

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

OGRGeometry* geom_from_line_draft(LPRENDERDEVICE device,
                                  const tool::Draft& draft, ushort line_type) {
  if (line_type == LT_LinearRing || draft.kind == tool::DraftKind::kPolygon) {
    OGRLinearRing* ring = new OGRLinearRing();
    if (draft.kind == tool::DraftKind::kRect) {
      add_rect_ring(device, draft, ring);
    } else {
      add_draft_points(device, draft, ring);
    }
    ring->closeRings();
    return ring;
  }
  if (line_type == LT_Spline_Lag || line_type == LT_Spline_Bzer ||
      line_type == LT_Spline_B || line_type == LT_Spline_3) {
    OGRLineString* spline = new OGRLineString();
    add_draft_points(device, draft, spline);
    return spline;
  }
  OGRLineString* line = new OGRLineString();
  if (draft.kind == tool::DraftKind::kRect || line_type == LT_Rect) {
    add_rect_ring(device, draft, line);
  } else {
    add_draft_points(device, draft, line);
  }
  return line;
}

OGRGeometry* geom_from_region_draft(LPRENDERDEVICE device,
                                    const tool::Draft& draft) {
  OGRLinearRing* ring = new OGRLinearRing();
  if (draft.kind == tool::DraftKind::kRect) {
    add_rect_ring(device, draft, ring);
  } else {
    add_draft_points(device, draft, ring);
  }
  ring->closeRings();
  OGRPolygon* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  return poly;
}
}  // namespace

void SmtAppendFeatureTool::apply_draft(const tool::Draft& draft) {
  if (!m_pRenderDevice || draft.points.empty()) {
    return;
  }

  int digitize_kind = m_digitizeKind;
  ushort point_type = m_pointType;
  ushort line_type = m_lineType;
  if (draft.flags != 0) {
    const uint32_t family = tool::draft_flags::family_of(draft.flags);
    const ushort code =
        static_cast<ushort>(tool::draft_flags::code_of(draft.flags));
    if (family == tool::draft_flags::kFamilyPoint) {
      digitize_kind = 1;
      point_type = code;
    } else if (family == tool::draft_flags::kFamilyLine) {
      digitize_kind = 2;
      line_type = code;
    } else if (family == tool::draft_flags::kFamilyRegion) {
      digitize_kind = 3;
    }
  }

  if (digitize_kind == 1) {
    float x = 0;
    float y = 0;
    m_pRenderDevice->DPToLP(draft.points[0].x_px, draft.points[0].y_px, x, y);
    m_pGeom = new OGRPoint(x, y);
    AppendPointFeature(point_type);
    return;
  }
  if (digitize_kind == 2) {
    m_pGeom = geom_from_line_draft(m_pRenderDevice, draft, line_type);
    AppendLineFeature();
    return;
  }
  if (digitize_kind == 3) {
    m_pGeom = geom_from_region_draft(m_pRenderDevice, draft);
    AppendRegionFeature();
  }
}

//////////////////////////////////////////////////////////////////////////
// Unbound fallback: SetActive on Append only (no BeginDelegate to Input*).
// Digitize without Workspace has no leftover pointer state machine — prefer
// bind_workspace so draw.* + apply_draft own the path.
void SmtAppendFeatureTool::OnInputPointFeature(ushort unType) {
  this->EndDelegate();
  m_digitizeKind = 1;
  m_pointType = unType;
  SetActive();
}

void SmtAppendFeatureTool::OnInputLineFeature(ushort unType) {
  this->EndDelegate();
  m_digitizeKind = 2;
  m_lineType = unType;
  SetActive();
}

void SmtAppendFeatureTool::OnInputRegionFeature(ushort unType) {
  this->EndDelegate();
  m_digitizeKind = 3;
  m_regionType = unType;
  SetActive();
}
//////////////////////////////////////////////////////////////////////////
void SmtAppendFeatureTool::AppendPointFeature(ushort unType) {
  switch (unType) {
    case PT_ChildImage: {
      AppendChildImageFeature();
    } break;
    case PT_Text: {
      if (SMT_ERR_NONE == SmtInputTextDlg(m_strAnno))
        AppendTextFeature(m_strAnno.c_str(), 0);
    } break;
    case PT_DOT: {
      AppendDotFeature();
    } break;
  }
}

void SmtAppendFeatureTool::AppendChildImageFeature() {
  if (m_pOperMap) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    OGRFeature* ogr =
        make_ogr_feature(m_pOperMap, m_pGeom, SmtFeatureType::SmtFtChildImage,
                         styleSonfig.szPointStyle);

    if (commit_append(m_edits.get(), ogr)) {
      fRect frt;
      Envelope envelope;
      OGRPoint* pPoint = (OGRPoint*)m_pGeom;
      float fMargin = 5. / m_pRenderDevice->GetBlc();

      geo::copy_envelope(*m_pGeom, &envelope);
      envelope.merge(pPoint->getX() - fMargin, pPoint->getY() - fMargin);
      envelope.merge(pPoint->getX() + fMargin, pPoint->getY() + fMargin);
      envelope_to_rect(frt, envelope);

      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
    SMT_SAFE_DELETE(m_pGeom);
  }
}

void SmtAppendFeatureTool::AppendTextFeature(const char* szAnno, float fangle) {
  if (m_pOperMap) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    OGRFeature* ogr =
        make_ogr_feature(m_pOperMap, m_pGeom, SmtFeatureType::SmtFtAnno,
                         styleSonfig.szPointStyle);
    set_field_string(ogr, "anno", szAnno);
    set_field_int(ogr, "color", int(RGB(0, 0, 0)));
    set_field_double(ogr, "angle", fangle);

    if (commit_append(m_edits.get(), ogr)) {
      fRect frt;
      Envelope envelope;
      OGRPoint* pPoint = (OGRPoint*)m_pGeom;
      float fMargin = 5. / m_pRenderDevice->GetBlc();

      geo::copy_envelope(*m_pGeom, &envelope);
      envelope.merge(pPoint->getX() - fMargin, pPoint->getY() - fMargin);
      envelope.merge(pPoint->getX() + fMargin, pPoint->getY() + fMargin);
      envelope_to_rect(frt, envelope);

      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
    SMT_SAFE_DELETE(m_pGeom);
  }
}

void SmtAppendFeatureTool::AppendDotFeature() {
  if (m_pOperMap) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    OGRFeature* ogr =
        make_ogr_feature(m_pOperMap, m_pGeom, SmtFeatureType::SmtFtDot,
                         styleSonfig.szPointStyle);

    if (commit_append(m_edits.get(), ogr)) {
      fRect frt;
      Envelope envelope;
      OGRPoint* pPoint = (OGRPoint*)m_pGeom;
      float fMargin = 5. / m_pRenderDevice->GetBlc();

      geo::copy_envelope(*m_pGeom, &envelope);
      envelope.merge(pPoint->getX() - fMargin, pPoint->getY() - fMargin);
      envelope.merge(pPoint->getX() + fMargin, pPoint->getY() + fMargin);
      envelope_to_rect(frt, envelope);

      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
    SMT_SAFE_DELETE(m_pGeom);
  }
}

void SmtAppendFeatureTool::AppendLineFeature(void) {
  if (m_pOperMap) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom, SmtFtCurve,
                                       styleSonfig.szLineStyle);
    if (auto* curve = dynamic_cast<OGRCurve*>(m_pGeom)) {
      set_field_double(ogr, "length", curve->get_Length());
    }

    if (commit_append(m_edits.get(), ogr)) {
      fRect frt;
      Envelope envelope;
      float fMargin = 5. / m_pRenderDevice->GetBlc();

      geo::copy_envelope(*m_pGeom, &envelope);
      envelope.merge(envelope.MinX - fMargin, envelope.MinY - fMargin);
      envelope.merge(envelope.MaxX + fMargin, envelope.MaxY + fMargin);
      envelope_to_rect(frt, envelope);

      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
    SMT_SAFE_DELETE(m_pGeom);
  }
}

void SmtAppendFeatureTool::AppendRegionFeature() {
  if (m_pOperMap) {
    SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
    SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

    OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom, SmtFtSurface,
                                       styleSonfig.szRegionStyle);
    if (auto* surf = dynamic_cast<OGRSurface*>(m_pGeom)) {
      set_field_double(ogr, "area", surf->get_Area());
    }

    if (commit_append(m_edits.get(), ogr)) {
      fRect frt;
      Envelope envelope;
      float fMargin = 5. / m_pRenderDevice->GetBlc();

      geo::copy_envelope(*m_pGeom, &envelope);
      envelope.merge(envelope.MinX - fMargin, envelope.MinY - fMargin);
      envelope.merge(envelope.MaxX + fMargin, envelope.MaxY + fMargin);
      envelope_to_rect(frt, envelope);

      m_pRenderDevice->Refresh(m_pOperMap, frt);
    }
    SMT_SAFE_DELETE(m_pGeom);
  }
}

int SmtAppendFeatureTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags) {
  {
    // if (GetKeyState(VK_CONTROL) & 0x80000000)
    {
      switch (nChar) {
        case 't':
        case 'T': {
          if (m_edits && m_edits->can_undo()) m_edits->undo();
        } break;
        case 'y':
        case 'Y': {
          if (m_edits && m_edits->can_redo()) m_edits->redo();
        } break;
      }
    }
  }

  return SmtBaseTool::KeyDown(nChar, nRepCnt, nFlags);
}
}  // namespace tool