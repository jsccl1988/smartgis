// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_PROJ_PROJECTION_H_
#define ALGORITHM_PROJ_PROJECTION_H_

#include "base/core/bas_struct.h"
#include "base/core/core.h"

#if defined(PROJ_EXPORTS)
#define PROJ_EXPORT_API __declspec(dllexport)
#else
#define PROJ_EXPORT_API __declspec(dllimport)
#endif

namespace geo {

struct GeoTransform {
  int need_geotransform;
  double rotation_angle;
  double geotransform[6];
  double invgeotransform[6];
};

// `proj` holds a PROJ 9 PJ* CRS created by process_projection.
// Callers treat it as opaque; do not put PJ* in sdb/crs.
struct Projection {
  int numargs;
  int automatic;
  char** args;
  void* proj;
  GeoTransform gt;
};

using SmtProjection = Projection;
using SmtGeoTransform = GeoTransform;

PROJ_EXPORT_API int project_point(Projection* in,
                                  Projection* out,
                                  base::dbfPoint* point);
PROJ_EXPORT_API int project_rect(Projection* in,
                                 Projection* out,
                                 base::dbfRect* rect);
PROJ_EXPORT_API int projections_differ(Projection*, Projection*);

PROJ_EXPORT_API void free_projection(Projection* p);
PROJ_EXPORT_API int init_projection(Projection* p);
PROJ_EXPORT_API int process_projection(Projection* p);
PROJ_EXPORT_API int load_projection_string(Projection* p, const char* value);
PROJ_EXPORT_API int load_projection_string_epsg(Projection* p,
                                                const char* value);

// IUGG 1975 (plugin Gauss/Lambert default) and Krasovsky ellipsoids.
constexpr double kIugg1975A = 6378140.0;
constexpr double kIugg1975B = 6356755.2882;
constexpr double kKrasovskyA = 6378245.0;
constexpr double kKrasovskyB = 6356863.0187730473;

PROJ_EXPORT_API double gauss_kruger_central_meridian(double lon_deg);

PROJ_EXPORT_API int load_longlat_ellipsoid(Projection* p, double a, double b);
PROJ_EXPORT_API int load_tmerc_crs(Projection* p,
                                   double a,
                                   double b,
                                   double lon_0);
PROJ_EXPORT_API int load_lcc_crs(Projection* p,
                                 double a,
                                 double b,
                                 double lat_1,
                                 double lat_2,
                                 double lat_0,
                                 double lon_0);
PROJ_EXPORT_API char* get_projection_string(Projection* proj);
PROJ_EXPORT_API void axis_normalize_points(Projection* proj,
                                           int count,
                                           double* x,
                                           double* y);
PROJ_EXPORT_API void axis_denormalize_points(Projection* proj,
                                             int count,
                                             double* x,
                                             double* y);

PROJ_EXPORT_API void set_proj_lib(const char*, const char*);

inline void SmtFreeProjection(Projection* p) { free_projection(p); }
inline double SmtGaussKrugerCentralMeridian(double lon_deg) {
  return gauss_kruger_central_meridian(lon_deg);
}
inline int SmtLoadLonglatEllipsoid(Projection* p, double a, double b) {
  return load_longlat_ellipsoid(p, a, b);
}
inline int SmtLoadTmercCrs(Projection* p, double a, double b, double lon_0) {
  return load_tmerc_crs(p, a, b, lon_0);
}

}  // namespace geo

#if !defined(PROJ_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "projD.lib")
#else
#pragma comment(lib, "proj.lib")
#endif
#endif

#endif  // ALGORITHM_PROJ_PROJECTION_H_
