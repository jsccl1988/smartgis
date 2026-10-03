// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_INSPECT_SYS_FLASH_STYLES_H_
#define LEGACY_UI_INSPECT_SYS_FLASH_STYLES_H_

// Resolves the six flash styles named by SmtStyleConfig for sys-config docks.

#include "legacy/gis/present/carto/style_api.h"
#include "legacy/gis/present/carto/stylemanager.h"

namespace legacy_ui {
namespace detail {

// Holds non-owning pointers to flash styles (may be null if lookup fails).
struct FlashStyles {
  SmtStyle* dot1 = nullptr;
  SmtStyle* line1 = nullptr;
  SmtStyle* region1 = nullptr;
  SmtStyle* dot2 = nullptr;
  SmtStyle* line2 = nullptr;
  SmtStyle* region2 = nullptr;
};

inline FlashStyles load_flash_styles(SmtStyleManager* style_mgr,
                                     const SmtStyleConfig& cfg) {
  FlashStyles out;
  if (style_mgr == nullptr) {
    return out;
  }
  out.dot1 = style_mgr->get_style(cfg.szDotFlashStyle1);
  out.line1 = style_mgr->get_style(cfg.szLineFlashStyle1);
  out.region1 = style_mgr->get_style(cfg.szRegionFlashStyle1);
  out.dot2 = style_mgr->get_style(cfg.szDotFlashStyle2);
  out.line2 = style_mgr->get_style(cfg.szLineFlashStyle2);
  out.region2 = style_mgr->get_style(cfg.szRegionFlashStyle2);
  return out;
}

inline void apply_flash_color1(const FlashStyles& s, COLORREF color) {
  if (s.dot1) {
    s.dot1->get_pen_desc().lPenColor = color;
  }
  if (s.line1) {
    s.line1->get_pen_desc().lPenColor = color;
  }
  if (s.region1) {
    s.region1->get_pen_desc().lPenColor = color;
  }
  if (s.dot2) {
    s.dot2->get_brush_desc().lBrushColor = color;
  }
  if (s.line2) {
    s.line2->get_brush_desc().lBrushColor = color;
  }
  if (s.region2) {
    s.region2->get_brush_desc().lBrushColor = color;
  }
}

inline void apply_flash_color2(const FlashStyles& s, COLORREF color) {
  if (s.dot1) {
    s.dot1->get_brush_desc().lBrushColor = color;
  }
  if (s.line1) {
    s.line1->get_brush_desc().lBrushColor = color;
  }
  if (s.region1) {
    s.region1->get_brush_desc().lBrushColor = color;
  }
  if (s.dot2) {
    s.dot2->get_pen_desc().lPenColor = color;
  }
  if (s.line2) {
    s.line2->get_pen_desc().lPenColor = color;
  }
  if (s.region2) {
    s.region2->get_pen_desc().lPenColor = color;
  }
}

}  // namespace detail
}  // namespace legacy_ui

#endif  // LEGACY_UI_INSPECT_SYS_FLASH_STYLES_H_
