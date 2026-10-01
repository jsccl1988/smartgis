// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/storm_surge.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "cpl_conv.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include "gis/analysis/raster/dem/flood_fill.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

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

bool json_get_int(const rapidjson::Value& obj, const char* key, int* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

bool map_to_pixel(const double* gt, double x, double y, int* col, int* row) {
  if (!gt || !col || !row) {
    return false;
  }
  Eigen::Matrix2d a;
  a << gt[1], gt[2], gt[4], gt[5];
  if (std::abs(a.determinant()) < 1e-18) {
    return false;
  }
  const Eigen::Vector2d pix =
      a.inverse() * Eigen::Vector2d(x - gt[0], y - gt[3]);
  *col = static_cast<int>(std::floor(pix(0)));
  *row = static_cast<int>(std::floor(pix(1)));
  return true;
}

bool write_byte_geotiff(const std::string& path,
                        int width,
                        int height,
                        const double* gt,
                        const unsigned char* mask) {
  if (path.empty() || width <= 0 || height <= 0 || !gt || !mask) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(driver->Create(path.c_str(), width, height, 1,
                                         GDT_Byte, nullptr));
  if (!ds) {
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  band->SetNoDataValue(0);
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<unsigned char*>(mask), width, height,
                        GDT_Byte, 0, 0, nullptr) == CE_None;
}

bool write_float_geotiff(const std::string& path,
                         int width,
                         int height,
                         const double* gt,
                         const float* values) {
  if (path.empty() || width <= 0 || height <= 0 || !gt || !values) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(driver->Create(path.c_str(), width, height, 1,
                                         GDT_Float32, nullptr));
  if (!ds) {
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  band->SetNoDataValue(0.0);
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<float*>(values), width, height, GDT_Float32,
                        0, 0, nullptr) == CE_None;
}

void append_geometry_points(OGRGeometry* geom, std::vector<double>* xy) {
  if (!geom || !xy) {
    return;
  }
  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());
  if (type == wkbPoint) {
    auto* pt = geom->toPoint();
    xy->push_back(pt->getX());
    xy->push_back(pt->getY());
    return;
  }
  if (type == wkbLineString || type == wkbLinearRing) {
    auto* line = geom->toLineString();
    const int n = line->getNumPoints();
    for (int i = 0; i < n; ++i) {
      xy->push_back(line->getX(i));
      xy->push_back(line->getY(i));
    }
    return;
  }
  if (type == wkbPolygon) {
    auto* poly = geom->toPolygon();
    append_geometry_points(poly->getExteriorRing(), xy);
    for (int i = 0; i < poly->getNumInteriorRings(); ++i) {
      append_geometry_points(poly->getInteriorRing(i), xy);
    }
    return;
  }
  if (type == wkbMultiPoint || type == wkbMultiLineString ||
      type == wkbMultiPolygon || type == wkbGeometryCollection) {
    auto* coll = dynamic_cast<OGRGeometryCollection*>(geom);
    if (!coll) {
      return;
    }
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      append_geometry_points(coll->getGeometryRef(i), xy);
    }
  }
}

bool load_coast_points(std::string_view coast_path, std::vector<double>* xy) {
  if (!xy || coast_path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(coast_path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
      nullptr, nullptr, nullptr)));
  if (!ds) {
    return false;
  }
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      append_geometry_points(feat->GetGeometryRef(), xy);
      OGRFeature::DestroyFeature(feat);
    }
  }
  return xy->size() >= 2;
}

bool load_series_file(std::string_view path, std::vector<double>* levels) {
  if (!levels || path.empty()) {
    return false;
  }
  const std::string path_str(path);
  std::ifstream in{path_str.c_str()};
  if (!in.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '/') {
      continue;
    }
    // First token on the line (CSV or whitespace).
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) {
      continue;
    }
    size_t end = line.find_first_of(", \t\r\n", start);
    if (end == std::string::npos) {
      end = line.size();
    }
    try {
      levels->push_back(std::stod(line.substr(start, end - start)));
    } catch (...) {
      continue;
    }
  }
  return !levels->empty();
}

void fill_depth_from_mask(const std::vector<float>& elev,
                          const std::vector<unsigned char>& mask,
                          double water_level,
                          std::vector<float>* depth_out) {
  if (!depth_out) {
    return;
  }
  depth_out->assign(mask.size(), 0.f);
  for (size_t i = 0; i < mask.size(); ++i) {
    if (!mask[i] || !std::isfinite(elev[i])) {
      continue;
    }
    const double d = water_level - static_cast<double>(elev[i]);
    (*depth_out)[i] = d > 0.0 ? static_cast<float>(d) : 0.f;
  }
}

StormSurgeResult surge_at_level(const std::vector<float>& elev,
                                int width,
                                int height,
                                const std::vector<DemFloodSeed>& seeds,
                                double water_level) {
  StormSurgeResult out;
  out.width = width;
  out.height = height;
  out.surge_levels = {water_level};
  const size_t flooded =
      flood_connected_at_level(elev, width, height, seeds, water_level, &out.mask);
  if (flooded == 0) {
    out.error = "empty_inundation";
    return out;
  }
  fill_depth_from_mask(elev, out.mask, water_level, &out.depth);
  out.ok = true;
  return out;
}

}  // namespace

StormSurgeResult run_storm_surge(std::string_view dem_path,
                                 std::string_view coast_path,
                                 const std::vector<double>& seed_xy,
                                 const std::vector<double>& surge_levels,
                                 int frames) {
  StormSurgeResult out;
  if (dem_path.empty()) {
    out.error = "empty_dem";
    return out;
  }
  if (surge_levels.empty()) {
    out.error = "empty_surge_levels";
    return out;
  }

  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(
      GDALOpen(std::string(dem_path).c_str(), GA_ReadOnly)));
  if (!ds) {
    out.error = "open_failed";
    return out;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    out.error = "no_band";
    return out;
  }
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  if (width <= 0 || height <= 0) {
    out.error = "bad_size";
    return out;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    gt[0] = 0;
    gt[1] = 1;
    gt[2] = 0;
    gt[3] = 0;
    gt[4] = 0;
    gt[5] = -1;
  }

  std::vector<float> elev(static_cast<size_t>(width) *
                          static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, elev.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    out.error = "read_failed";
    return out;
  }
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  if (nodata_ok) {
    for (float& v : elev) {
      if (std::abs(static_cast<double>(v) - nodata) < 1e-9 ||
          !std::isfinite(v)) {
        v = std::numeric_limits<float>::infinity();
      }
    }
  }

  std::vector<double> map_seeds = seed_xy;
  if (!coast_path.empty()) {
    if (!load_coast_points(coast_path, &map_seeds)) {
      out.error = "coast_load_failed";
      return out;
    }
  }
  if (map_seeds.size() < 2) {
    out.error = "no_ocean_seeds";
    return out;
  }

  std::vector<DemFloodSeed> seeds;
  seeds.reserve(map_seeds.size() / 2);
  for (size_t i = 0; i + 1 < map_seeds.size(); i += 2) {
    int col = 0;
    int row = 0;
    if (!map_to_pixel(gt, map_seeds[i], map_seeds[i + 1], &col, &row)) {
      out.error = "bad_geotransform";
      return out;
    }
    if (col < 0 || row < 0 || col >= width || row >= height) {
      continue;
    }
    seeds.push_back(DemFloodSeed{col, row});
  }
  if (seeds.empty()) {
    out.error = "seeds_out_of_bounds";
    return out;
  }

  double seed_min_z = std::numeric_limits<double>::infinity();
  for (const DemFloodSeed& s : seeds) {
    const size_t idx =
        static_cast<size_t>(s.row) * static_cast<size_t>(width) +
        static_cast<size_t>(s.col);
    if (std::isfinite(elev[idx])) {
      seed_min_z = std::min(seed_min_z, static_cast<double>(elev[idx]));
    }
  }
  if (!std::isfinite(seed_min_z)) {
    out.error = "seed_nodata";
    return out;
  }

  std::vector<double> levels = surge_levels;
  const int frame_count = std::max(1, frames);
  if (levels.size() == 1 && frame_count > 1) {
    const double z1 = levels[0];
    levels.clear();
    levels.reserve(static_cast<size_t>(frame_count));
    for (int f = 0; f < frame_count; ++f) {
      const double t =
          static_cast<double>(f + 1) / static_cast<double>(frame_count);
      levels.push_back(seed_min_z + (z1 - seed_min_z) * t);
    }
  }

  for (int i = 0; i < 6; ++i) {
    out.geotransform[i] = gt[i];
  }
  out.width = width;
  out.height = height;

  if (levels.size() == 1) {
    out = surge_at_level(elev, width, height, seeds, levels[0]);
    for (int i = 0; i < 6; ++i) {
      out.geotransform[i] = gt[i];
    }
    return out;
  }

  out.frame_masks.reserve(levels.size());
  out.frame_depths.reserve(levels.size());
  out.surge_levels = levels;
  for (double level : levels) {
    StormSurgeResult frame =
        surge_at_level(elev, width, height, seeds, level);
    if (!frame.ok) {
      out.error = frame.error.empty() ? "frame_failed" : frame.error;
      return out;
    }
    out.frame_masks.push_back(std::move(frame.mask));
    out.frame_depths.push_back(std::move(frame.depth));
  }
  out.mask = out.frame_masks.back();
  out.depth = out.frame_depths.back();
  out.ok = true;
  return out;
}

bool write_storm_surge_mask_geotiff(std::string_view output_path,
                                    const StormSurgeResult& result,
                                    std::string_view frames_dir) {
  if (!result.ok || result.mask.empty()) {
    return false;
  }
  if (!write_byte_geotiff(std::string(output_path), result.width, result.height,
                          result.geotransform, result.mask.data())) {
    return false;
  }
  if (frames_dir.empty() || result.frame_masks.empty()) {
    return true;
  }
  for (size_t i = 0; i < result.frame_masks.size(); ++i) {
    const std::string frame_path =
        std::string(frames_dir) + "/frame_" + std::to_string(i) + ".tif";
    if (!write_byte_geotiff(frame_path, result.width, result.height,
                            result.geotransform,
                            result.frame_masks[i].data())) {
      return false;
    }
  }
  return true;
}

bool write_storm_surge_depth_geotiff(std::string_view output_path,
                                     const StormSurgeResult& result,
                                     std::string_view frames_dir) {
  if (!result.ok || result.depth.empty()) {
    return false;
  }
  if (!write_float_geotiff(std::string(output_path), result.width,
                           result.height, result.geotransform,
                           result.depth.data())) {
    return false;
  }
  if (frames_dir.empty() || result.frame_depths.empty()) {
    return true;
  }
  for (size_t i = 0; i < result.frame_depths.size(); ++i) {
    const std::string frame_path =
        std::string(frames_dir) + "/depth_frame_" + std::to_string(i) + ".tif";
    if (!write_float_geotiff(frame_path, result.width, result.height,
                             result.geotransform,
                             result.frame_depths[i].data())) {
      return false;
    }
  }
  return true;
}

bool polygonize_storm_surge_mask(std::string_view output_path,
                                 const StormSurgeResult& result) {
  if (!result.ok || result.mask.empty() || output_path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* mem_drv = GetGDALDriverManager()->GetDriverByName("MEM");
  GDALDriver* json_drv = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!mem_drv || !json_drv) {
    return false;
  }
  GDALDatasetUniquePtr raster(mem_drv->Create("", result.width, result.height,
                                              1, GDT_Byte, nullptr));
  if (!raster) {
    return false;
  }
  raster->SetGeoTransform(const_cast<double*>(result.geotransform));
  GDALRasterBand* band = raster->GetRasterBand(1);
  if (!band ||
      band->RasterIO(GF_Write, 0, 0, result.width, result.height,
                     const_cast<unsigned char*>(result.mask.data()),
                     result.width, result.height, GDT_Byte, 0, 0,
                     nullptr) != CE_None) {
    return false;
  }

  const std::string path(output_path);
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr vec(json_drv->Create(path.c_str(), 0, 0, 0, GDT_Unknown,
                                            nullptr));
  if (!vec) {
    return false;
  }
  OGRLayer* layer = vec->CreateLayer("inundation", nullptr, wkbPolygon, nullptr);
  if (!layer) {
    return false;
  }
  OGRFieldDefn field("DN", OFTInteger);
  if (layer->CreateField(&field) != OGRERR_NONE) {
    return false;
  }
  // Mask band skips zeros; DN field stores the pixel value (1 = wet).
  const CPLErr err = GDALPolygonize(GDALRasterBand::ToHandle(band),
                                    GDALRasterBand::ToHandle(band),
                                    OGRLayer::ToHandle(layer), 0, nullptr,
                                    nullptr, nullptr);
  if (err != CE_None) {
    return false;
  }
  vec.reset();  // flush GeoJSON to disk
  // Non-empty output file is enough; GeoJSON feature counts can be -1 until reopen.
  VSIStatBufL st = {};
  return VSIStatL(path.c_str(), &st) == 0 && st.st_size > 0;
}

bool run_storm_surge_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string dem;
  std::string output;
  if (!json_get_string(args, "dem", &dem) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  std::string coast;
  if (!json_get_string(args, "coast", &coast)) {
    json_get_string(args, "shoreline", &coast);
  }

  std::vector<double> seed_xy;
  double seed_x = 0;
  double seed_y = 0;
  if (json_get_double(args, "seed_x", &seed_x) &&
      json_get_double(args, "seed_y", &seed_y)) {
    seed_xy.push_back(seed_x);
    seed_xy.push_back(seed_y);
  }

  std::vector<double> levels;
  const auto levels_it = args.FindMember("surge_levels");
  if (levels_it != args.MemberEnd() && levels_it->value.IsArray()) {
    for (const auto& v : levels_it->value.GetArray()) {
      if (v.IsNumber()) {
        levels.push_back(v.GetDouble());
      }
    }
  }
  if (levels.empty()) {
    std::string series_path;
    if (json_get_string(args, "tide", &series_path) ||
        json_get_string(args, "series", &series_path) ||
        json_get_string(args, "surge_series", &series_path)) {
      if (!load_series_file(series_path, &levels)) {
        return false;
      }
    }
  }
  if (levels.empty()) {
    double water_level = 0;
    double water_depth = 0;
    const bool has_level = json_get_double(args, "water_level", &water_level);
    const bool has_depth = json_get_double(args, "water_depth", &water_depth);
    if (has_level) {
      levels.push_back(water_level);
    } else if (has_depth) {
      // Resolve absolute level from first coast/seed DEM sample after open —
      // defer via a temporary seed-relative marker encoded as depth offset.
      // Sample DEM at first available seed after coast load inside run.
      // For op path: open DEM, sample first seed after coast points collected.
      GDALAllRegister();
      GDALDatasetUniquePtr ds(
          static_cast<GDALDataset*>(GDALOpen(dem.c_str(), GA_ReadOnly)));
      if (!ds) {
        return false;
      }
      double gt[6] = {};
      if (ds->GetGeoTransform(gt) != CE_None) {
        return false;
      }
      std::vector<double> map_seeds = seed_xy;
      if (!coast.empty()) {
        load_coast_points(coast, &map_seeds);
      }
      if (map_seeds.size() < 2) {
        return false;
      }
      int col = 0;
      int row = 0;
      if (!map_to_pixel(gt, map_seeds[0], map_seeds[1], &col, &row)) {
        return false;
      }
      GDALRasterBand* band = ds->GetRasterBand(1);
      float z = 0;
      if (!band ||
          band->RasterIO(GF_Read, col, row, 1, 1, &z, 1, 1, GDT_Float32, 0, 0,
                         nullptr) != CE_None) {
        return false;
      }
      levels.push_back(static_cast<double>(z) + water_depth);
    } else {
      return false;
    }
  }

  if (coast.empty() && seed_xy.size() < 2) {
    return false;
  }

  int frames = 1;
  json_get_int(args, "frames", &frames);
  std::string frames_dir;
  json_get_string(args, "frames_dir", &frames_dir);

  const StormSurgeResult result =
      run_storm_surge(dem, coast, seed_xy, levels, frames);
  if (!result.ok) {
    return false;
  }
  if (!write_storm_surge_mask_geotiff(output, result, frames_dir)) {
    return false;
  }
  std::string depth_output;
  if (json_get_string(args, "depth_output", &depth_output)) {
    if (!write_storm_surge_depth_geotiff(depth_output, result, frames_dir)) {
      return false;
    }
  }
  std::string polygons_output;
  if (json_get_string(args, "polygons_output", &polygons_output)) {
    if (!polygonize_storm_surge_mask(polygons_output, result)) {
      return false;
    }
  }
  return true;
}

StormSurgeWaterMesh build_storm_surge_water_mesh(const unsigned char* mask,
                                                 int width,
                                                 int height,
                                                 const double* geotransform,
                                                 double water_level,
                                                 const float* depth,
                                                 int max_dim) {
  StormSurgeWaterMesh mesh;
  if (!mask || !geotransform || width <= 0 || height <= 0 || max_dim < 1) {
    return mesh;
  }
  const double gt0 = geotransform[0];
  const double gt1 = geotransform[1];
  const double gt2 = geotransform[2];
  const double gt3 = geotransform[3];
  const double gt4 = geotransform[4];
  const double gt5 = geotransform[5];

  const int step_x = std::max(1, (width + max_dim - 1) / max_dim);
  const int step_y = std::max(1, (height + max_dim - 1) / max_dim);
  const int ncol = (width + step_x - 1) / step_x;
  const int nrow = (height + step_y - 1) / step_y;
  if (ncol < 1 || nrow < 1) {
    return mesh;
  }

  const size_t vert_slots =
      static_cast<size_t>(ncol + 1) * static_cast<size_t>(nrow + 1);
  std::vector<int> vert_id(vert_slots, -1);
  mesh.xyz.reserve(vert_slots * 3);
  mesh.indices.reserve(static_cast<size_t>(ncol) * static_cast<size_t>(nrow) *
                       6);

  auto pixel_xy = [&](int col, int row, double* x, double* y) {
    *x = gt0 + gt1 * col + gt2 * row;
    *y = gt3 + gt4 * col + gt5 * row;
  };

  auto ensure_vert = [&](int ic, int ir) -> int {
    const size_t key =
        static_cast<size_t>(ir) * static_cast<size_t>(ncol + 1) +
        static_cast<size_t>(ic);
    if (vert_id[key] >= 0) {
      return vert_id[key];
    }
    const int pixel_c = std::min(ic * step_x, width);
    const int pixel_r = std::min(ir * step_y, height);
    double x = 0;
    double y = 0;
    pixel_xy(pixel_c, pixel_r, &x, &y);
    // Free surface at surge level; depth confirms wetness only (Z constant).
    double z = water_level;
    if (depth) {
      const int sample_c = std::min(pixel_c, width - 1);
      const int sample_r = std::min(pixel_r, height - 1);
      const size_t di =
          static_cast<size_t>(sample_r) * static_cast<size_t>(width) +
          static_cast<size_t>(sample_c);
      if (depth[di] > 0.f) {
        z = water_level;
      }
    }
    const int idx = static_cast<int>(mesh.xyz.size() / 3);
    mesh.xyz.push_back(x);
    mesh.xyz.push_back(y);
    mesh.xyz.push_back(z);
    vert_id[key] = idx;
    return idx;
  };

  for (int ir = 0; ir < nrow; ++ir) {
    for (int ic = 0; ic < ncol; ++ic) {
      const int c = ic * step_x;
      const int r = ir * step_y;
      if (c >= width || r >= height) {
        continue;
      }
      const size_t idx =
          static_cast<size_t>(r) * static_cast<size_t>(width) +
          static_cast<size_t>(c);
      if (!mask[idx]) {
        continue;
      }
      const int v00 = ensure_vert(ic, ir);
      const int v10 = ensure_vert(ic + 1, ir);
      const int v11 = ensure_vert(ic + 1, ir + 1);
      const int v01 = ensure_vert(ic, ir + 1);
      // CCW when gt5 < 0 (north-up GeoTIFF); both windings paint as fill.
      mesh.indices.push_back(v00);
      mesh.indices.push_back(v10);
      mesh.indices.push_back(v11);
      mesh.indices.push_back(v00);
      mesh.indices.push_back(v11);
      mesh.indices.push_back(v01);
    }
  }
  return mesh;
}

StormSurgeWaterMesh build_storm_surge_water_mesh(const StormSurgeResult& result,
                                                 int frame_index,
                                                 int max_dim) {
  StormSurgeWaterMesh empty;
  if (!result.ok || result.width <= 0 || result.height <= 0) {
    return empty;
  }
  int fi = frame_index;
  if (fi < 0) {
    if (!result.frame_masks.empty()) {
      fi = static_cast<int>(result.frame_masks.size()) - 1;
    } else {
      fi = 0;
    }
  }
  const unsigned char* mask = result.mask.data();
  const float* depth =
      result.depth.empty() ? nullptr : result.depth.data();
  double water_level = 0.0;
  if (!result.surge_levels.empty()) {
    water_level = result.surge_levels.back();
  }
  if (!result.frame_masks.empty()) {
    if (fi < 0 || fi >= static_cast<int>(result.frame_masks.size())) {
      return empty;
    }
    mask = result.frame_masks[static_cast<size_t>(fi)].data();
    if (fi < static_cast<int>(result.frame_depths.size()) &&
        !result.frame_depths[static_cast<size_t>(fi)].empty()) {
      depth = result.frame_depths[static_cast<size_t>(fi)].data();
    }
    if (fi < static_cast<int>(result.surge_levels.size())) {
      water_level = result.surge_levels[static_cast<size_t>(fi)];
    }
  } else if (result.mask.empty()) {
    return empty;
  }
  return build_storm_surge_water_mesh(mask, result.width, result.height,
                                      result.geotransform, water_level, depth,
                                      max_dim);
}

}  // namespace detail
}  // namespace gis
