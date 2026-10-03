// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/chrome/aux_overlay.h"

#include <vector>

#include "legacy/core/types/point.h"
#include "legacy/core/types/rect.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/sys/sysmanager.h"

namespace ui {
namespace detail {

void paint_aux_overlay(render::LPRENDERDEVICE device,
                       const tool::AuxOverlay* overlay) {
  if (!device || !overlay || overlay->points.size() < 2 ||
      overlay->kind == tool::AuxOverlay::Kind::kNone) {
    return;
  }
  sys::SmtSysManager* sys_mgr = sys::SmtSysManager::get_singleton_ptr();
  SmtStyleManager* styles = SmtStyleManager::get_singleton_ptr();
  if (!sys_mgr || !styles) {
    return;
  }
  SmtStyleConfig cfg = sys_mgr->get_sys_style_config();
  SmtStyle* style = styles->get_style(cfg.szAuxStyle);
  if (SMT_ERR_NONE !=
      device->BeginRender(render::MRD_BL_DIRECT, false, style, R2_COPYPEN)) {
    return;
  }
  if (overlay->kind == tool::AuxOverlay::Kind::kRect) {
    base::fRect frt;
    frt.merge(overlay->points[0].x_px, overlay->points[0].y_px);
    frt.merge(overlay->points[1].x_px, overlay->points[1].y_px);
    device->DrawRect(frt, true);
  } else if (overlay->kind == tool::AuxOverlay::Kind::kPolyline) {
    std::vector<base::fPoint> pts;
    pts.reserve(overlay->points.size());
    for (const tool::AuxPoint& p : overlay->points) {
      pts.push_back(
          base::fPoint(static_cast<float>(p.x_px), static_cast<float>(p.y_px)));
    }
    device->DrawLine(pts.data(), static_cast<int>(pts.size()), true);
  }
  device->EndRender(render::MRD_BL_DIRECT);
}

}  // namespace detail
}  // namespace ui
