// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <span>
#include <vector>

#include "base/math/simd.h"
#include "gis/model/envelope.h"
#include "legacy/core/types/types.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "ogrsf_frmts.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace detail {

namespace {

void project_xy_batch(const LpToDp2& xform, const float* xy_in, int n_pts,
                      POINT* out) {
  transform_xy_batch(
      xform,
      std::span<const float>(xy_in, static_cast<size_t>(n_pts) * 2u),
      std::span<long>(reinterpret_cast<long*>(out),
                      static_cast<size_t>(n_pts) * 2u));
}

}  // namespace

int GdiMeshDraw::draw_tin(const SmtTin* tin) {
  c_->draw_tin_lines(tin);

  // if (c_->rd_options_->bShowPoint)
  {
    c_->draw_tin_nodes(tin);
  }

  return SMT_ERR_NONE;
}

int GdiMeshDraw::draw_tin_lines(const SmtTin* tin) {
  POINT lPt1, lPt2, lPt3;
  OGRPoint oPt1, oPt2, oPt3;

  Envelope envTri, envViewp;
  lRect lViewp;
  fRect fViewp;

  viewport_to_rect(lViewp, c_->rc_->viewport);
  c_->drect_to_lrect(lViewp, fViewp);
  rect_to_envelope(envViewp, fViewp);

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  float xy[6];
  POINT pts[3];

  for (int i = 0; i < tin->get_triangle_count(); i++) {
    SmtTriangle tri = tin->get_triangle(i);

    if (!tri.bDelete) {
      oPt1 = tin->get_point(tri.a);
      oPt2 = tin->get_point(tri.b);
      oPt3 = tin->get_point(tri.c);

      envTri.merge(oPt1.getX(), oPt1.getY());
      envTri.merge(oPt2.getX(), oPt2.getY());
      envTri.merge(oPt3.getX(), oPt3.getY());

      if (envTri.intersects(envViewp)) {
        xy[0] = static_cast<float>(oPt1.getX());
        xy[1] = static_cast<float>(oPt1.getY());
        xy[2] = static_cast<float>(oPt2.getX());
        xy[3] = static_cast<float>(oPt2.getY());
        xy[4] = static_cast<float>(oPt3.getX());
        xy[5] = static_cast<float>(oPt3.getY());
        project_xy_batch(xform, xy, 3, pts);
        lPt1 = pts[0];
        lPt2 = pts[1];
        lPt3 = pts[2];

        MoveToEx(c_->h_cur_dc_, lPt1.x, lPt1.y, nullptr);
        LineTo(c_->h_cur_dc_, lPt2.x, lPt2.y);
        LineTo(c_->h_cur_dc_, lPt3.x, lPt3.y);
        LineTo(c_->h_cur_dc_, lPt1.x, lPt1.y);
      }
    }
  }

  return SMT_ERR_NONE;
}

int GdiMeshDraw::draw_tin_nodes(const SmtTin* tin) {
  POINT lPt;
  OGRPoint oPt;
  int r = c_->rd_options_->lPointRaduis;

  lRect lViewp;
  fRect fViewp;

  viewport_to_rect(lViewp, c_->rc_->viewport);
  c_->drect_to_lrect(lViewp, fViewp);
  fViewp.normalize();

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  for (int i = 0; i < tin->get_point_count(); i++) {
    oPt = tin->get_point(i);
    if (fViewp.contains(static_cast<float>(oPt.getX()),
                        static_cast<float>(oPt.getY()))) {
      transform_xy(xform, static_cast<float>(oPt.getX()),
                   static_cast<float>(oPt.getY()), &lPt.x, &lPt.y);
      Ellipse(c_->h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
    }
  }

  return SMT_ERR_NONE;
}

int GdiMeshDraw::draw_grid(const SmtGrid* grid) {
  c_->draw_grid_lines(grid);

  // if (c_->rd_options_->bShowPoint)
  {
    c_->draw_grid_nodes(grid);
  }

  return SMT_ERR_NONE;
}

int GdiMeshDraw::draw_grid_lines(const SmtGrid* grid) {
  int nM, nN;
  grid->get_size(nM, nN);

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> pts;

  for (int j = 0; j < nN; j++) {
    xy.resize(static_cast<size_t>(nM) * 2u);
    pts.resize(static_cast<size_t>(nM));
    for (int i = 0; i < nM; i++) {
      RawPoint rawPt = grid->node(i, j);
      xy[static_cast<size_t>(i) * 2u] = rawPt.x;
      xy[static_cast<size_t>(i) * 2u + 1u] = rawPt.y;
    }
    project_xy_batch(xform, xy.data(), nM, pts.data());
    MoveToEx(c_->h_cur_dc_, pts[0].x, pts[0].y, nullptr);
    for (int i = 1; i < nM; i++) {
      LineTo(c_->h_cur_dc_, pts[static_cast<size_t>(i)].x,
             pts[static_cast<size_t>(i)].y);
    }
  }

  for (int i = 0; i < nM; i++) {
    xy.resize(static_cast<size_t>(nN) * 2u);
    pts.resize(static_cast<size_t>(nN));
    for (int j = 0; j < nN; j++) {
      RawPoint rawPt = grid->node(i, j);
      xy[static_cast<size_t>(j) * 2u] = rawPt.x;
      xy[static_cast<size_t>(j) * 2u + 1u] = rawPt.y;
    }
    project_xy_batch(xform, xy.data(), nN, pts.data());
    MoveToEx(c_->h_cur_dc_, pts[0].x, pts[0].y, nullptr);
    for (int j = 1; j < nN; j++) {
      LineTo(c_->h_cur_dc_, pts[static_cast<size_t>(j)].x,
             pts[static_cast<size_t>(j)].y);
    }
  }

  return SMT_ERR_NONE;
}

int GdiMeshDraw::draw_grid_nodes(const SmtGrid* grid) {
  int nM, nN;
  grid->get_size(nM, nN);

  int r = c_->rd_options_->lPointRaduis;
  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> pts;
  const int n = nM * nN;
  xy.resize(static_cast<size_t>(n) * 2u);
  pts.resize(static_cast<size_t>(n));
  int k = 0;
  for (int j = 0; j < nN; j++) {
    for (int i = 0; i < nM; i++) {
      RawPoint rawPt = grid->node(i, j);
      xy[static_cast<size_t>(k) * 2u] = rawPt.x;
      xy[static_cast<size_t>(k) * 2u + 1u] = rawPt.y;
      ++k;
    }
  }
  project_xy_batch(xform, xy.data(), n, pts.data());
  for (int i = 0; i < n; ++i) {
    const POINT& lPt = pts[static_cast<size_t>(i)];
    Ellipse(c_->h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
  }

  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace render
