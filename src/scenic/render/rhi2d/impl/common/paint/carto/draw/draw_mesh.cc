// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"

#include <span>
#include <vector>

#include "base/math/simd/simd.h"
#include "gis/geo/ops/indexed_tin.h"
#include "base/math/math.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "ogrsf_frmts.h"

using namespace gis;
using namespace base;

namespace scenic {
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

int GdiMeshDraw::draw_tin(const OGRTriangulatedSurface* tin) {
  c_->draw_tin_lines(tin);

  // if (c_->rd_options_->bShowPoint)
  {
    c_->draw_tin_nodes(tin);
  }

  return kErrNone;
}

int GdiMeshDraw::draw_tin_lines(const OGRTriangulatedSurface* tin) {
  if (!tin) {
    return kErrInvalidParam;
  }
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

  const int ntri = const_cast<OGRTriangulatedSurface*>(tin)->getNumGeometries();
  for (int i = 0; i < ntri; i++) {
    if (!geo::tin_patch_points(*tin, i, &oPt1, &oPt2, &oPt3)) {
      continue;
    }

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

  return kErrNone;
}

int GdiMeshDraw::draw_tin_nodes(const OGRTriangulatedSurface* tin) {
  if (!tin) {
    return kErrInvalidParam;
  }
  POINT lPt;
  OGRPoint oPt1, oPt2, oPt3;
  int r = c_->rd_options_->lPointRaduis;

  lRect lViewp;
  fRect fViewp;

  viewport_to_rect(lViewp, c_->rc_->viewport);
  c_->drect_to_lrect(lViewp, fViewp);
  fViewp.normalize();

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  const int ntri = const_cast<OGRTriangulatedSurface*>(tin)->getNumGeometries();
  auto draw_pt = [&](const OGRPoint& oPt) {
    if (fViewp.contains(static_cast<float>(oPt.getX()),
                        static_cast<float>(oPt.getY()))) {
      transform_xy(xform, static_cast<float>(oPt.getX()),
                   static_cast<float>(oPt.getY()), &lPt.x, &lPt.y);
      Ellipse(c_->h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
    }
  };
  for (int i = 0; i < ntri; i++) {
    if (!geo::tin_patch_points(*tin, i, &oPt1, &oPt2, &oPt3)) {
      continue;
    }
    draw_pt(oPt1);
    draw_pt(oPt2);
    draw_pt(oPt3);
  }

  return kErrNone;
}

int GdiMeshDraw::draw_grid(const plugin::detail::OrthoLattice* grid) {
  c_->draw_grid_lines(grid);

  // if (c_->rd_options_->bShowPoint)
  {
    c_->draw_grid_nodes(grid);
  }

  return kErrNone;
}

int GdiMeshDraw::draw_grid_lines(const plugin::detail::OrthoLattice* grid) {
  if (!grid || grid->is_empty()) {
    return kErrInvalidParam;
  }
  const int nM = grid->ny;
  const int nN = grid->nx;

  const LpToDp2 xform = make_lp_to_dp(*c_->rc_);
  thread_local std::vector<float> xy;
  thread_local std::vector<POINT> pts;

  for (int j = 0; j < nN; j++) {
    xy.resize(static_cast<size_t>(nM) * 2u);
    pts.resize(static_cast<size_t>(nM));
    for (int i = 0; i < nM; i++) {
      double px = 0;
      double py = 0;
      grid->ortho_point(j, i, &px, &py);
      xy[static_cast<size_t>(i) * 2u] = static_cast<float>(px);
      xy[static_cast<size_t>(i) * 2u + 1u] = static_cast<float>(py);
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
      double px = 0;
      double py = 0;
      grid->ortho_point(j, i, &px, &py);
      xy[static_cast<size_t>(j) * 2u] = static_cast<float>(px);
      xy[static_cast<size_t>(j) * 2u + 1u] = static_cast<float>(py);
    }
    project_xy_batch(xform, xy.data(), nN, pts.data());
    MoveToEx(c_->h_cur_dc_, pts[0].x, pts[0].y, nullptr);
    for (int j = 1; j < nN; j++) {
      LineTo(c_->h_cur_dc_, pts[static_cast<size_t>(j)].x,
             pts[static_cast<size_t>(j)].y);
    }
  }

  return kErrNone;
}

int GdiMeshDraw::draw_grid_nodes(const plugin::detail::OrthoLattice* grid) {
  if (!grid || grid->is_empty()) {
    return kErrInvalidParam;
  }
  const int nM = grid->ny;
  const int nN = grid->nx;

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
      double px = 0;
      double py = 0;
      grid->ortho_point(j, i, &px, &py);
      xy[static_cast<size_t>(k) * 2u] = static_cast<float>(px);
      xy[static_cast<size_t>(k) * 2u + 1u] = static_cast<float>(py);
      ++k;
    }
  }
  project_xy_batch(xform, xy.data(), n, pts.data());
  for (int i = 0; i < n; ++i) {
    const POINT& lPt = pts[static_cast<size_t>(i)];
    Ellipse(c_->h_cur_dc_, lPt.x - r, lPt.y - r, lPt.x + r, lPt.y + r);
  }

  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
