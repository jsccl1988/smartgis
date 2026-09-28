// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_PROJ_PROJECTION_H_
#define ALGORITHM_PROJ_PROJECTION_H_


#include "gis/gis_export.h"
#include "legacy/core/bas_struct.h"
#include "legacy/core/core.h"

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

GIS_EXPORT int project_point(Projection* in,
                                  Projection* out,
                                  base::dbfPoint* point);
GIS_EXPORT int project_rect(Projection* in,
                                 Projection* out,
                                 base::dbfRect* rect);
GIS_EXPORT int projections_differ(Projection*, Projection*);

GIS_EXPORT void free_projection(Projection* p);
GIS_EXPORT int init_projection(Projection* p);
GIS_EXPORT int process_projection(Projection* p);
GIS_EXPORT int load_projection_string(Projection* p, const char* value);
GIS_EXPORT int load_projection_string_epsg(Projection* p,
                                                const char* value);

// IUGG 1975 (plugin Gauss/Lambert default) and Krasovsky ellipsoids.
constexpr double kIugg1975A = 6378140.0;
constexpr double kIugg1975B = 6356755.2882;
constexpr double kKrasovskyA = 6378245.0;
constexpr double kKrasovskyB = 6356863.0187730473;

GIS_EXPORT double gauss_kruger_central_meridian(double lon_deg);

GIS_EXPORT int load_longlat_ellipsoid(Projection* p, double a, double b);
GIS_EXPORT int load_tmerc_crs(Projection* p,
                                   double a,
                                   double b,
                                   double lon_0);
GIS_EXPORT int load_lcc_crs(Projection* p,
                                 double a,
                                 double b,
                                 double lat_1,
                                 double lat_2,
                                 double lat_0,
                                 double lon_0);
GIS_EXPORT char* get_projection_string(Projection* proj);
GIS_EXPORT void axis_normalize_points(Projection* proj,
                                           int count,
                                           double* x,
                                           double* y);
GIS_EXPORT void axis_denormalize_points(Projection* proj,
                                             int count,
                                             double* x,
                                             double* y);

GIS_EXPORT void set_proj_lib(const char*, const char*);

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


#endif  // ALGORITHM_PROJ_PROJECTION_H_
