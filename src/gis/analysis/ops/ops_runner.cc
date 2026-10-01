// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/ops_runner.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/stats.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"
#include "gis/analysis/geometry/fit.h"
#include "gis/analysis/network/cost_path.h"
#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/dem/flood_fill.h"
#include "gis/analysis/raster/dem/storm_surge.h"
#include "gis/analysis/raster/dem/storm_surge_stats.h"
#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_smooth.h"

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include "gis/kernel/geo/ops/geo_ops.h"
#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

const std::vector<BuiltinOpDesc> kCatalog = {
    {"native.buffer", "Buffer"},
    {"native.clip", "Clip"},
    {"native.centroid", "Centroid"},
    {"native.envelope", "Envelope"},
    {"native.intersection", "Intersection"},
    {"native.union", "Union"},
    {"native.difference", "Difference"},
    {"native.symmetric_difference", "Symmetric difference"},
    {"native.simplify", "Simplify"},
    {"native.convex_hull", "Convex hull"},
    {"native.boundary", "Boundary"},
    {"native.cost_path", "Least-cost / shortest path"},
    {"native.flood_fill", "DEM inundation flood fill"},
    {"native.storm_surge", "Coastal storm-surge inundation"},
    {"native.storm_surge_stats",
     "Storm-surge inundation area / depth-class / overlap stats"},
    {"native.fit_line", "Least-squares 2D line fit (Eigen SVD)"},
    {"native.fit_plane", "Least-squares 3D plane fit (Eigen SVD)"},
    {"native.affine_align", "2D affine alignment (Eigen least squares)"},
    {"native.dem_gradient", "DEM slope/aspect (Eigen Map)"},
    {"native.raster_convolve", "Raster convolution (box3)"},
    {"native.raster_smooth", "Raster Laplace smooth (Eigen SparseLU)"},
    {"native.stratum_interpolate", "Stratum surface TIN from borehole CSV"},
    {"native.stratum_prism_volume", "Stratum prism volume (top/bottom)"},
    {"native.geochem_stats", "Geochem sample stats + histogram"},
    {"native.geochem_idw", "Geochem IDW anomaly surface"},
};

void ignore_geos_message(const char* /*fmt*/, ...) {}

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool write_geometry_geojson(const std::string& path, OGRGeometry* geom) {
  if (!geom || path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown,
                                         nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("result", nullptr, wkbUnknown, nullptr);
  if (!layer) {
    return false;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
  if (!feat) {
    return false;
  }
  feat->SetGeometry(geom);
  return layer->CreateFeature(feat.get()) == OGRERR_NONE;
}

std::unique_ptr<OGRGeometry> load_first_geometry(const std::string& path) {
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr)));
  if (!ds) {
    return nullptr;
  }
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      OGRGeometry* geom = feat->GetGeometryRef();
      if (geom && !geom->IsEmpty()) {
        OGRGeometry* clone = geom->clone();
        OGRFeature::DestroyFeature(feat);
        return std::unique_ptr<OGRGeometry>(clone);
      }
      OGRFeature::DestroyFeature(feat);
    }
  }
  return nullptr;
}

OGRGeometry* envelope_polygon(const OGRGeometry& geom) {
  OGREnvelope env;
  const_cast<OGRGeometry&>(geom).getEnvelope(&env);
  auto* ring = new OGRLinearRing();
  ring->addPoint(env.MinX, env.MinY);
  ring->addPoint(env.MaxX, env.MinY);
  ring->addPoint(env.MaxX, env.MaxY);
  ring->addPoint(env.MinX, env.MaxY);
  ring->closeRings();
  auto* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  return poly;
}

GEOSGeometry* to_geos(GEOSContextHandle_t ctx, const OGRGeometry& geom) {
  const int wkb_size = const_cast<OGRGeometry&>(geom).WkbSize();
  if (wkb_size <= 0) {
    return nullptr;
  }
  std::vector<unsigned char> wkb(static_cast<size_t>(wkb_size));
  if (const_cast<OGRGeometry&>(geom).exportToWkb(wkbNDR, wkb.data()) !=
      OGRERR_NONE) {
    return nullptr;
  }
  return GEOSGeomFromWKB_buf_r(ctx, wkb.data(), static_cast<size_t>(wkb_size));
}

OGRGeometry* from_geos(GEOSContextHandle_t ctx, const GEOSGeometry* geom) {
  if (!ctx || !geom) {
    return nullptr;
  }
  size_t n = 0;
  unsigned char* wkb = GEOSGeomToWKB_buf_r(ctx, geom, &n);
  if (!wkb || n == 0) {
    return nullptr;
  }
  OGRGeometry* out = nullptr;
  if (OGRGeometryFactory::createFromWkb(wkb, nullptr, &out,
                                        static_cast<int>(n)) != OGRERR_NONE) {
    out = nullptr;
  }
  GEOSFree_r(ctx, wkb);
  return out;
}

enum class GeosBinary { kIntersection, kUnion, kDifference, kSymDifference };

OGRGeometry* geos_binary(const OGRGeometry& a,
                         const OGRGeometry& b,
                         GeosBinary op) {
  GEOSContextHandle_t ctx = GEOS_init_r();
  if (!ctx) {
    return nullptr;
  }
  GEOSContext_setNoticeHandler_r(ctx, &ignore_geos_message);
  GEOSContext_setErrorHandler_r(ctx, &ignore_geos_message);
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* gb = to_geos(ctx, b);
  GEOSGeometry* result = nullptr;
  if (ga && gb) {
    switch (op) {
      case GeosBinary::kIntersection:
        result = GEOSIntersection_r(ctx, ga, gb);
        break;
      case GeosBinary::kUnion:
        result = GEOSUnion_r(ctx, ga, gb);
        break;
      case GeosBinary::kDifference:
        result = GEOSDifference_r(ctx, ga, gb);
        break;
      case GeosBinary::kSymDifference:
        result = GEOSSymDifference_r(ctx, ga, gb);
        break;
    }
  }
  OGRGeometry* out = from_geos(ctx, result);
  if (result) {
    GEOSGeom_destroy_r(ctx, result);
  }
  if (ga) {
    GEOSGeom_destroy_r(ctx, ga);
  }
  if (gb) {
    GEOSGeom_destroy_r(ctx, gb);
  }
  GEOS_finish_r(ctx);
  return out;
}

OGRGeometry* geos_unary(const OGRGeometry& a,
                        GEOSGeometry* (*fn)(GEOSContextHandle_t,
                                            const GEOSGeometry*),
                        double simplify_tol = 0.0) {
  GEOSContextHandle_t ctx = GEOS_init_r();
  if (!ctx) {
    return nullptr;
  }
  GEOSContext_setNoticeHandler_r(ctx, &ignore_geos_message);
  GEOSContext_setErrorHandler_r(ctx, &ignore_geos_message);
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* result = nullptr;
  if (ga) {
    if (fn) {
      result = fn(ctx, ga);
    } else {
      result = GEOSSimplify_r(ctx, ga, simplify_tol);
    }
  }
  OGRGeometry* out = from_geos(ctx, result);
  if (result) {
    GEOSGeom_destroy_r(ctx, result);
  }
  if (ga) {
    GEOSGeom_destroy_r(ctx, ga);
  }
  GEOS_finish_r(ctx);
  return out;
}

OGRGeometry* geos_centroid(const OGRGeometry& a) {
  GEOSContextHandle_t ctx = GEOS_init_r();
  if (!ctx) {
    return nullptr;
  }
  GEOSContext_setNoticeHandler_r(ctx, &ignore_geos_message);
  GEOSContext_setErrorHandler_r(ctx, &ignore_geos_message);
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* result = ga ? GEOSGetCentroid_r(ctx, ga) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  if (result) {
    GEOSGeom_destroy_r(ctx, result);
  }
  if (ga) {
    GEOSGeom_destroy_r(ctx, ga);
  }
  GEOS_finish_r(ctx);
  return out;
}

std::unique_ptr<OGRGeometry> apply_op(std::string_view id,
                                      OGRGeometry* input,
                                      OGRGeometry* clip,
                                      double distance) {
  if (!input) {
    return nullptr;
  }
  if (id == "native.buffer") {
    OGRGeometry* out = geo::buffer_via_geos_or_ogr(*input, distance);
    return std::unique_ptr<OGRGeometry>(out);
  }
  if (id == "native.clip" || id == "native.intersection") {
    if (!clip) {
      return nullptr;
    }
    return std::unique_ptr<OGRGeometry>(
        geos_binary(*input, *clip, GeosBinary::kIntersection));
  }
  if (id == "native.centroid") {
    return std::unique_ptr<OGRGeometry>(geos_centroid(*input));
  }
  if (id == "native.envelope") {
    return std::unique_ptr<OGRGeometry>(envelope_polygon(*input));
  }
  if (id == "native.union") {
    if (!clip) {
      return nullptr;
    }
    return std::unique_ptr<OGRGeometry>(
        geos_binary(*input, *clip, GeosBinary::kUnion));
  }
  if (id == "native.difference") {
    if (!clip) {
      return nullptr;
    }
    return std::unique_ptr<OGRGeometry>(
        geos_binary(*input, *clip, GeosBinary::kDifference));
  }
  if (id == "native.symmetric_difference") {
    if (!clip) {
      return nullptr;
    }
    return std::unique_ptr<OGRGeometry>(
        geos_binary(*input, *clip, GeosBinary::kSymDifference));
  }
  if (id == "native.simplify") {
    const double tol = distance > 0.0 ? distance : 0.01;
    return std::unique_ptr<OGRGeometry>(geos_unary(*input, nullptr, tol));
  }
  if (id == "native.convex_hull") {
    return std::unique_ptr<OGRGeometry>(
        geos_unary(*input, &GEOSConvexHull_r));
  }
  if (id == "native.boundary") {
    return std::unique_ptr<OGRGeometry>(
        geos_unary(*input, &GEOSBoundary_r));
  }
  return nullptr;
}

}  // namespace

const std::vector<BuiltinOpDesc>& builtin_op_catalog() {
  return kCatalog;
}

bool run_builtin_op(std::string_view processing_id,
                    std::string_view args_json) {
  if (processing_id == "native.cost_path") {
    return run_cost_path_op(args_json);
  }
  if (processing_id == "native.flood_fill") {
    return run_flood_fill_op(args_json);
  }
  if (processing_id == "native.storm_surge") {
    return run_storm_surge_op(args_json);
  }
  if (processing_id == "native.storm_surge_stats") {
    return run_storm_surge_stats_op(args_json);
  }
  if (processing_id == "native.fit_line") {
    return run_fit_line_op(args_json);
  }
  if (processing_id == "native.fit_plane") {
    return run_fit_plane_op(args_json);
  }
  if (processing_id == "native.affine_align") {
    return run_affine_align_op(args_json);
  }
  if (processing_id == "native.dem_gradient") {
    return run_dem_gradient_op(args_json);
  }
  if (processing_id == "native.raster_convolve") {
    return run_raster_convolve_op(args_json);
  }
  if (processing_id == "native.raster_smooth") {
    return run_raster_smooth_op(args_json);
  }
  if (processing_id == "native.stratum_interpolate") {
    return run_stratum_interpolate_op(args_json);
  }
  if (processing_id == "native.stratum_prism_volume") {
    return run_stratum_prism_volume_op(args_json);
  }
  if (processing_id == "native.geochem_stats") {
    return run_geochem_stats_op(args_json);
  }
  if (processing_id == "native.geochem_idw") {
    return run_geochem_idw_op(args_json);
  }

  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  double distance = 0.01;
  (void)json_get_double(args, "distance", &distance);
  if (!(distance > 0.0) &&
      (processing_id == "native.buffer" ||
       processing_id == "native.simplify")) {
    distance = 0.01;
  }

  auto geom = load_first_geometry(input);
  if (!geom) {
    return false;
  }

  std::unique_ptr<OGRGeometry> clip;
  std::string clip_path;
  if (json_get_string(args, "clip", &clip_path)) {
    clip = load_first_geometry(clip_path);
  }

  const bool needs_clip =
      processing_id == "native.clip" ||
      processing_id == "native.intersection" ||
      processing_id == "native.union" ||
      processing_id == "native.difference" ||
      processing_id == "native.symmetric_difference";
  if (needs_clip && !clip) {
    return false;
  }

  auto result = apply_op(processing_id, geom.get(), clip.get(), distance);
  if (!result || result->IsEmpty()) {
    return false;
  }
  return write_geometry_geojson(output, result.get());
}

}  // namespace detail
}  // namespace gis
