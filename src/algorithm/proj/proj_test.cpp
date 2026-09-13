// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/proj/proj_backend.h"
#include "algorithm/proj/projection.h"

// transform_xy is a DLL export (not a header-only traits instantiation).

#include <cmath>
#include <cstdio>

using base::dbfPoint;
using geo::free_projection;
using geo::gauss_kruger_central_meridian;
using geo::init_projection;
using geo::load_lcc_crs;
using geo::load_longlat_ellipsoid;
using geo::load_projection_string;
using geo::load_projection_string_epsg;
using geo::load_tmerc_crs;
using geo::project_point;
using geo::Projection;
using geo::projections_differ;
using geo::set_proj_lib;
using geo::kIugg1975A;
using geo::kIugg1975B;

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  expect(init_projection(nullptr) == SMT_ERR_INVALID_PARAM,
         "init rejects null");

  Projection src = {};
  Projection dst = {};
  expect(init_projection(&src) == SMT_ERR_NONE, "init src");
  expect(init_projection(&dst) == SMT_ERR_NONE, "init dst");
  expect(projections_differ(&src, &dst) == 0, "empty defs do not differ");

  expect(load_projection_string_epsg(&src, "4326") == SMT_ERR_NONE,
         "load EPSG:4326");
  expect(load_projection_string_epsg(&dst, "3857") == SMT_ERR_NONE,
         "load EPSG:3857");
  expect(src.proj != nullptr, "4326 compiled");
  expect(dst.proj != nullptr, "3857 compiled");
  expect(projections_differ(&src, &dst) == 1, "4326 differs from 3857");

  dbfPoint origin(0, 0);
  expect(project_point(&src, &dst, &origin) == SMT_ERR_NONE,
         "0,0 4326->3857");
  expect(std::fabs(origin.x) < 1e-6 && std::fabs(origin.y) < 1e-6,
         "0,0 stays at origin in 3857");

  dbfPoint beijing_area(120.0, 30.0);
  expect(project_point(&src, &dst, &beijing_area) == SMT_ERR_NONE,
         "120,30 4326->3857");
  expect(std::fabs(beijing_area.x - 13358338.89) < 2.0, "3857 X");
  expect(std::fabs(beijing_area.y - 3503549.84) < 2.0, "3857 Y");

  // Spec lon/lat; EPSG:3857 (sphere R=6378137) metres, 1 m tolerance.
  // The design note (12958573, 4853957) is ~2 km off this pair.
  dbfPoint beijing(116.3883, 39.9289);
  expect(project_point(&src, &dst, &beijing) == SMT_ERR_NONE,
         "116.3883,39.9289 4326->3857");
  expect(std::fabs(beijing.x - 12956286.3) < 1.0, "beijing 3857 X");
  expect(std::fabs(beijing.y - 4855615.6) < 1.0, "beijing 3857 Y");

  double tx = 116.3883;
  double ty = 39.9289;
  expect(proj::transform_xy("EPSG:4326", "EPSG:3857", &tx, &ty) == SMT_ERR_NONE,
         "transform_xy 4326->3857");
  expect(std::fabs(tx - 12956286.3) < 1.0 && std::fabs(ty - 4855615.6) < 1.0,
         "transform_xy beijing");
  expect(proj::transform_xy("", "EPSG:3857", &tx, &ty) != SMT_ERR_NONE,
         "transform_xy empty src");

  Projection empty_crs = {};
  expect(init_projection(&empty_crs) == SMT_ERR_NONE, "init empty crs");
  expect(load_projection_string(&empty_crs, "") != 0, "empty CRS fails");
  expect(load_projection_string(&empty_crs, "   ") != 0,
         "whitespace CRS fails");

  dbfPoint same(10.0, 20.0);
  expect(project_point(&src, &src, &same) == SMT_ERR_NONE, "identity 4326");
  expect(std::fabs(same.x - 10.0) < 1e-9 && std::fabs(same.y - 20.0) < 1e-9,
         "identity leaves point");

  Projection bad = {};
  init_projection(&bad);
  expect(load_projection_string_epsg(&bad, "999999") == SMT_ERR_FAILURE,
         "invalid EPSG fails");

  expect(std::fabs(gauss_kruger_central_meridian(117.0) - 117.0) < 1e-9,
         "zone 20 CM is 117");

  Projection geo = {};
  Projection gk = {};
  expect(init_projection(&geo) == SMT_ERR_NONE, "init longlat");
  expect(init_projection(&gk) == SMT_ERR_NONE, "init tmerc");
  expect(load_longlat_ellipsoid(&geo, kIugg1975A, kIugg1975B) == SMT_ERR_NONE,
         "IUGG1975 longlat");
  expect(load_tmerc_crs(&gk, kIugg1975A, kIugg1975B,
                       gauss_kruger_central_meridian(117.0)) == SMT_ERR_NONE,
         "IUGG1975 tmerc zone 20");

  dbfPoint gauss(117.0, 36.0);
  expect(project_point(&geo, &gk, &gauss) == SMT_ERR_NONE, "117,36 -> tmerc");
  expect(std::isfinite(gauss.x) && std::isfinite(gauss.y), "gauss finite");
  expect(gauss.x > 499000.0 && gauss.x < 501000.0, "gauss easting ~500000");
  expect(gauss.y > 3000000.0 && gauss.y < 5000000.0, "gauss northing");

  Projection lcc = {};
  expect(init_projection(&lcc) == SMT_ERR_NONE, "init lcc");
  expect(load_lcc_crs(&lcc, kIugg1975A, kIugg1975B, 38.0, 41.0, 39.5, 116.0) ==
             SMT_ERR_NONE,
         "IUGG1975 lcc");
  dbfPoint lambert(116.0, 39.5);
  expect(project_point(&geo, &lcc, &lambert) == SMT_ERR_NONE, "lcc project");
  expect(std::isfinite(lambert.x) && std::isfinite(lambert.y), "lcc finite");

  free_projection(&src);
  free_projection(&dst);
  free_projection(&empty_crs);
  free_projection(&bad);
  free_projection(&geo);
  free_projection(&gk);
  free_projection(&lcc);
  set_proj_lib(nullptr, nullptr);

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
