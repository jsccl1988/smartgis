// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/tool/select/select_query_apply.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatoolmanager.h"
#include "legacy/tool/abi/t_msg.h"
#include "legacy/ui/dialogs/dialogs_api.h"
#include "ogrsf_frmts.h"

namespace tool {
namespace {

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

}  // namespace

void clear_select_scratch(gis::ScratchLayer& scratch) {
  gis::DataSourceMgr::destroy_mem_vec_layer(scratch);
  scratch = gis::DataSourceMgr::create_mem_vec_layer();
}

float select_query_margin_lp(render::LPRENDERDEVICE device, double dp_margin) {
  if (!device) {
    return 0.05f;
  }
  const double blc = device->GetBlc();
  if (blc <= 1e-12) {
    return 0.05f;
  }
  return static_cast<float>(dp_margin / blc);
}

void refresh_select_fea_type(SmtMap* map, int& fea_type) {
  if (!map) {
    return;
  }
  fea_type = gis::datasource::feature_type_of(map->GetActiveOgrLayer());
}

void post_select_flash_data(HWND hwnd, gis::ScratchLayer* scratch,
                            int* fea_type) {
  SmtListenerMsg param;
  param.hSrcWnd = hwnd;
  param.wParam = WPARAM(scratch);
  param.lParam = LPARAM(fea_type);
  post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                   SMT_MSG_KEY(GT_MSG_SET_FLASH_DATA, hwnd), param);
}

void post_select_flash_start(HWND hwnd) {
  SmtListenerMsg param;
  param.hSrcWnd = hwnd;
  post_ia_tool_msg(SMT_IATOOL_MSG_BROADCAST,
                   SMT_MSG_KEY(GT_MSG_START_FLASH, hwnd), param);
}

OGRGeometry* query_geom_from_select_draft(render::LPRENDERDEVICE device,
                                          const Draft& draft, bool as_circle) {
  if (!device || draft.points.empty()) {
    return nullptr;
  }

  auto to_lp = [device](const DraftPoint& p, float& x, float& y) {
    device->DPToLP(p.x_px, p.y_px, x, y);
  };

  if (draft.kind == DraftKind::kPoint) {
    float x = 0;
    float y = 0;
    to_lp(draft.points[0], x, y);
    return new OGRPoint(x, y);
  }

  if (draft.kind == DraftKind::kRect && draft.points.size() >= 2) {
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    to_lp(draft.points[0], x0, y0);
    to_lp(draft.points[1], x1, y1);

    if (as_circle || draft_flags::is_select_circle(draft.flags)) {
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
      return poly;
    }

    OGRLinearRing* ring = new OGRLinearRing();
    ring->addPoint(x0, y0);
    ring->addPoint(x1, y0);
    ring->addPoint(x1, y1);
    ring->addPoint(x0, y1);
    ring->closeRings();
    OGRPolygon* poly = new OGRPolygon();
    poly->addRingDirectly(ring);
    return poly;
  }

  OGRLinearRing* ring = new OGRLinearRing();
  for (const DraftPoint& p : draft.points) {
    float x = 0;
    float y = 0;
    to_lp(p, x, y);
    ring->addPoint(x, y);
  }
  ring->closeRings();
  return ring;
}

void run_select_query(render::LPRENDERDEVICE device, SmtMap* map,
                      gis::ScratchLayer& scratch, SmtGQueryDesc& gq,
                      SmtPQueryDesc& pq, int& fea_type, double dp_margin,
                      HWND hwnd, bool point_query) {
  if (!(GetAsyncKeyState(VK_LCONTROL) & 0x8000)) {
    clear_select_scratch(scratch);
  }

  gq.fSmargin = select_query_margin_lp(device, dp_margin);
  if (scratch.layer && map) {
    map->QueryFeature(&gq, &pq, scratch.layer, fea_type);
  }
  if (fea_type == SmtFtUnknown) {
    refresh_select_fea_type(map, fea_type);
  }

  SMT_SAFE_DELETE(gq.pQueryGeom);

  post_select_flash_data(hwnd, &scratch, &fea_type);
  post_select_flash_start(hwnd);

  if (point_query) {
    return;
  }

  const int count = ogr_feature_count(scratch.layer);
  if (count > 1) {
    uint unID = SMT_C_INVALID_UINT_VALUE;
    std::vector<uint> vIDs;
    collect_fids(scratch.layer, vIDs);
    SmtSelectOneDlg(unID, vIDs);
  }
}

}  // namespace tool
