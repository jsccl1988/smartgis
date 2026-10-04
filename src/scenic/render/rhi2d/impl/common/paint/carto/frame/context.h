// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_CONTEXT_H_
#define SCENIC_GDI_CONTEXT_H_

#include "gis/map/map.h"
#include "scenic/detail/viewport.h"
#include "scenic/detail/err.h"

using namespace base;

namespace scenic {
namespace detail {

// Leftover GDI FrameJob paint inputs (viewport + map slice).
struct RenderContext {
  base::Viewport viewport;
  base::Windowport windowport;
  float fblc;
  gis::Map* pMap;
  int orgx, orgy;
  int width, height;
  int op;

  RenderContext()
      : fblc(1), pMap(nullptr), orgx(0), orgy(0), width(0), height(0), op(0) {}

  RenderContext(base::Viewport vp, base::Windowport wp, float _fblc,
                  const gis::Map* _pMap, int _x, int _y, int _w, int _h,
                  int _op = R2_COPYPEN)
      : viewport(vp),
        windowport(wp),
        fblc(_fblc),
        pMap(const_cast<gis::Map*>(_pMap)),
        orgx(_x),
        orgy(_y),
        width(_w),
        height(_h),
        op(_op) {}
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_CONTEXT_H_
