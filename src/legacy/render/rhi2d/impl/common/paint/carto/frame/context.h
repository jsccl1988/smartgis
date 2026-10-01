// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_CONTEXT_H_
#define SMT_LEGACY_RENDER_GDI_CONTEXT_H_

#include "gis/model/map/map.h"
#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/core/macros/macros.h"

using namespace base;

namespace render {

// Leftover GDI FrameJob paint inputs (viewport + map slice).
struct SmtRenderContext {
  base::Viewport viewport;
  base::Windowport windowport;
  float fblc;
  gis::SmtMap* pMap;
  int orgx, orgy;
  int width, height;
  int op;

  SmtRenderContext()
      : fblc(1), pMap(nullptr), orgx(0), orgy(0), width(0), height(0), op(0) {}

  SmtRenderContext(base::Viewport vp, base::Windowport wp, float _fblc,
                  const gis::SmtMap* _pMap, int _x, int _y, int _w, int _h,
                  int _op = R2_COPYPEN)
      : viewport(vp),
        windowport(wp),
        fblc(_fblc),
        pMap(const_cast<gis::SmtMap*>(_pMap)),
        orgx(_x),
        orgy(_y),
        width(_w),
        height(_h),
        op(_op) {}
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_CONTEXT_H_
