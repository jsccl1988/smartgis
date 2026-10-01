// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/storm_surge_stats.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace detail {
namespace {

constexpr double kDefaultBreaks[] = {0.5, 1.0, 2.0};

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

std::vector<double> default_depth_breaks() {
  return {kDefaultBreaks[0], kDefaultBreaks[1], kDefaultBreaks[2]};
}

std::vector<double> parse_depth_breaks(const rapidjson::Value& args) {
  std::vector<double> breaks;
  const auto it = args.FindMember("depth_breaks");
  if (it != args.MemberEnd() && it->value.IsArray()) {
    for (const auto& v : it->value.GetArray()) {
      if (v.IsNumber()) {
        breaks.push_back(v.GetDouble());
      }
    }
  }
  if (breaks.empty()) {
    return default_depth_breaks();
  }
  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
  return breaks;
}

bool read_byte_mask(std::string_view path,
                    std::vector<unsigned char>* mask,
                    int* width,
                    int* height,
                    double* gt) {
  if (!mask || !width || !height || !gt || path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(
      GDALOpen(std::string(path).c_str(), GA_ReadOnly)));
  if (!ds) {
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  *width = ds->GetRasterXSize();
  *height = ds->GetRasterYSize();
  if (*width <= 0 || *height <= 0) {
    return false;
  }
  if (ds->GetGeoTransform(gt) != CE_None) {
    gt[0] = 0;
    gt[1] = 1;
    gt[2] = 0;
    gt[3] = 0;
    gt[4] = 0;
    gt[5] = -1;
  }
  mask->assign(static_cast<size_t>(*width) * static_cast<size_t>(*height), 0);
  return band->RasterIO(GF_Read, 0, 0, *width, *height, mask->data(), *width,
                        *height, GDT_Byte, 0, 0, nullptr) == CE_None;
}

bool read_float_depth(std::string_view path,
                      int expect_w,
                      int expect_h,
                      std::vector<float>* depth) {
  if (!depth || path.empty() || expect_w <= 0 || expect_h <= 0) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(
      GDALOpen(std::string(path).c_str(), GA_ReadOnly)));
  if (!ds) {
    return false;
  }
  if (ds->GetRasterXSize() != expect_w || ds->GetRasterYSize() != expect_h) {
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  depth->assign(static_cast<size_t>(expect_w) * static_cast<size_t>(expect_h),
                0.f);
  return band->RasterIO(GF_Read, 0, 0, expect_w, expect_h, depth->data(),
                        expect_w, expect_h, GDT_Float32, 0, 0,
                        nullptr) == CE_None;
}

bool write_byte_geotiff(const std::string& path,
                        int width,
                        int height,
                        const double* gt,
                        const unsigned char* values) {
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
                        const_cast<unsigned char*>(values), width, height,
                        GDT_Byte, 0, 0, nullptr) == CE_None;
}

// Build MultiPolygon of wet cells via GDALPolygonize into memory GeoJSON.
std::unique_ptr<OGRGeometry> polygonize_mask(const unsigned char* mask,
                                             int width,
                                             int height,
                                             const double* gt) {
  if (!mask || width <= 0 || height <= 0 || !gt) {
    return nullptr;
  }
  GDALAllRegister();
  GDALDriver* mem_drv = GetGDALDriverManager()->GetDriverByName("MEM");
  if (!mem_drv) {
    return nullptr;
  }
  GDALDatasetUniquePtr raster(
      mem_drv->Create("", width, height, 1, GDT_Byte, nullptr));
  if (!raster) {
    return nullptr;
  }
  raster->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = raster->GetRasterBand(1);
  if (!band ||
      band->RasterIO(GF_Write, 0, 0, width, height,
                     const_cast<unsigned char*>(mask), width, height, GDT_Byte,
                     0, 0, nullptr) != CE_None) {
    return nullptr;
  }

  GDALDriver* mem_vec = GetGDALDriverManager()->GetDriverByName("Memory");
  if (!mem_vec) {
    return nullptr;
  }
  GDALDatasetUniquePtr vec(
      mem_vec->Create("", 0, 0, 0, GDT_Unknown, nullptr));
  if (!vec) {
    return nullptr;
  }
  OGRLayer* layer = vec->CreateLayer("inundation", nullptr, wkbPolygon, nullptr);
  if (!layer) {
    return nullptr;
  }
  OGRFieldDefn field("DN", OFTInteger);
  if (layer->CreateField(&field) != OGRERR_NONE) {
    return nullptr;
  }
  if (GDALPolygonize(GDALRasterBand::ToHandle(band),
                     GDALRasterBand::ToHandle(band), OGRLayer::ToHandle(layer),
                     0, nullptr, nullptr, nullptr) != CE_None) {
    return nullptr;
  }

  auto* multi = new OGRMultiPolygon();
  layer->ResetReading();
  OGRFeature* feat = nullptr;
  while ((feat = layer->GetNextFeature()) != nullptr) {
    OGRGeometry* g = feat->StealGeometry();
    if (g) {
      const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
      if (t == wkbPolygon) {
        multi->addGeometryDirectly(g);
      } else if (t == wkbMultiPolygon) {
        auto* mp = g->toMultiPolygon();
        for (int i = 0; i < mp->getNumGeometries(); ++i) {
          multi->addGeometry(mp->getGeometryRef(i));
        }
        delete g;
      } else {
        delete g;
      }
    }
    OGRFeature::DestroyFeature(feat);
  }
  if (multi->getNumGeometries() == 0) {
    delete multi;
    return nullptr;
  }
  return std::unique_ptr<OGRGeometry>(multi);
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
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
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

double geometry_area(OGRGeometry* geom) {
  if (!geom || geom->IsEmpty()) {
    return 0;
  }
  // Area lives on curve/surface subclasses; use C API for OGRGeometry*.
  return std::abs(OGR_G_Area(OGRGeometry::ToHandle(geom)));
}

StormSurgeOverlapStats compute_overlap(const unsigned char* mask,
                                       int width,
                                       int height,
                                       const double* gt,
                                       std::string_view impact_path,
                                       double buffer_distance,
                                       std::string_view buffer_output,
                                       std::string_view overlap_output) {
  StormSurgeOverlapStats out;
  out.buffer_distance = buffer_distance;
  auto inundation = polygonize_mask(mask, width, height, gt);
  if (!inundation) {
    out.error = "polygonize_failed";
    return out;
  }
  out.inundation_area = geometry_area(inundation.get());

  std::unique_ptr<OGRGeometry> zone;
  if (buffer_distance > 0.0) {
    zone.reset(inundation->Buffer(buffer_distance));
    if (!zone) {
      out.error = "buffer_failed";
      return out;
    }
    out.buffer_area = geometry_area(zone.get());
    if (!buffer_output.empty() &&
        !write_geometry_geojson(std::string(buffer_output), zone.get())) {
      out.error = "buffer_write_failed";
      return out;
    }
  } else {
    zone = std::move(inundation);
    out.buffer_area = out.inundation_area;
  }

  if (impact_path.empty()) {
    out.computed = true;
    return out;
  }

  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(impact_path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
      nullptr, nullptr, nullptr)));
  if (!ds) {
    out.error = "impact_open_failed";
    return out;
  }

  GDALDriver* mem_drv = GetGDALDriverManager()->GetDriverByName("Memory");
  GDALDatasetUniquePtr overlap_ds;
  OGRLayer* overlap_layer = nullptr;
  if (!overlap_output.empty() && mem_drv) {
    overlap_ds.reset(mem_drv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
    if (overlap_ds) {
      overlap_layer =
          overlap_ds->CreateLayer("overlap", nullptr, wkbUnknown, nullptr);
    }
  }

  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* layer = ds->GetLayer(li);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      ++out.impact_feature_count;
      OGRGeometry* g = feat->GetGeometryRef();
      if (!g || g->IsEmpty()) {
        OGRFeature::DestroyFeature(feat);
        continue;
      }
      std::unique_ptr<OGRGeometry> inter(g->Intersection(zone.get()));
      if (inter && !inter->IsEmpty()) {
        ++out.intersect_feature_count;
        out.intersect_area += geometry_area(inter.get());
        if (overlap_layer) {
          OGRFeatureUniquePtr copy(
              OGRFeature::CreateFeature(overlap_layer->GetLayerDefn()));
          if (copy) {
            copy->SetGeometry(inter.get());
            overlap_layer->CreateFeature(copy.get());
          }
        }
      }
      OGRFeature::DestroyFeature(feat);
    }
  }

  if (!overlap_output.empty() && overlap_ds && overlap_layer) {
    GDALDriver* json_drv = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    if (!json_drv) {
      out.error = "overlap_driver_missing";
      return out;
    }
    VSIUnlink(std::string(overlap_output).c_str());
    GDALDatasetUniquePtr out_ds(json_drv->Create(
        std::string(overlap_output).c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    if (!out_ds) {
      out.error = "overlap_write_failed";
      return out;
    }
    OGRLayer* out_layer =
        out_ds->CreateLayer("overlap", nullptr, wkbUnknown, nullptr);
    if (!out_layer) {
      out.error = "overlap_write_failed";
      return out;
    }
    overlap_layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = overlap_layer->GetNextFeature()) != nullptr) {
      out_layer->CreateFeature(feat);
      OGRFeature::DestroyFeature(feat);
    }
  }

  out.computed = true;
  return out;
}

void write_stats_json(const StormSurgeStatsResult& result,
                      const std::string& path) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(result.ok);
  w.Key("width");
  w.Int(result.width);
  w.Key("height");
  w.Int(result.height);
  w.Key("wet_cells");
  w.Int(result.wet_cells);
  w.Key("cell_area");
  w.Double(result.cell_area);
  w.Key("inundation_area");
  w.Double(result.inundation_area);
  w.Key("depth_classes");
  w.StartArray();
  for (const StormSurgeDepthClass& c : result.depth_classes) {
    w.StartObject();
    w.Key("lo");
    w.Double(c.lo);
    w.Key("hi");
    if (c.unbounded_hi) {
      w.String("inf");
    } else {
      w.Double(c.hi);
    }
    w.Key("cell_count");
    w.Int(c.cell_count);
    w.Key("area");
    w.Double(c.area);
    w.EndObject();
  }
  w.EndArray();
  if (result.overlap.computed || !result.overlap.error.empty()) {
    w.Key("overlap");
    w.StartObject();
    w.Key("computed");
    w.Bool(result.overlap.computed);
    w.Key("buffer_distance");
    w.Double(result.overlap.buffer_distance);
    w.Key("inundation_area");
    w.Double(result.overlap.inundation_area);
    w.Key("buffer_area");
    w.Double(result.overlap.buffer_area);
    w.Key("impact_feature_count");
    w.Int(result.overlap.impact_feature_count);
    w.Key("intersect_feature_count");
    w.Int(result.overlap.intersect_feature_count);
    w.Key("intersect_area");
    w.Double(result.overlap.intersect_area);
    if (!result.overlap.error.empty()) {
      w.Key("error");
      w.String(result.overlap.error.c_str());
    }
    w.EndObject();
  }
  if (!result.error.empty()) {
    w.Key("error");
    w.String(result.error.c_str());
  }
  w.EndObject();

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(buf.GetString(), static_cast<std::streamsize>(buf.GetSize()));
}

}  // namespace

double storm_surge_cell_area(const double* geotransform) {
  if (!geotransform) {
    return 0;
  }
  return std::abs(geotransform[1] * geotransform[5] -
                  geotransform[2] * geotransform[4]);
}

bool compute_inundation_area(const unsigned char* mask,
                             int width,
                             int height,
                             const double* geotransform,
                             int* wet_cells,
                             double* area) {
  if (!mask || width <= 0 || height <= 0 || !geotransform || !wet_cells ||
      !area) {
    return false;
  }
  const size_t n =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  int wet = 0;
  for (size_t i = 0; i < n; ++i) {
    if (mask[i]) {
      ++wet;
    }
  }
  const double cell = storm_surge_cell_area(geotransform);
  *wet_cells = wet;
  *area = static_cast<double>(wet) * cell;
  return true;
}

bool classify_storm_surge_depth(const float* depth,
                                const unsigned char* mask,
                                int width,
                                int height,
                                const double* geotransform,
                                const std::vector<double>& depth_breaks,
                                std::vector<StormSurgeDepthClass>* classes,
                                std::vector<unsigned char>* class_mask_out) {
  if (!depth || !mask || width <= 0 || height <= 0 || !geotransform ||
      !classes) {
    return false;
  }
  std::vector<double> breaks = depth_breaks;
  if (breaks.empty()) {
    breaks = default_depth_breaks();
  }
  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());

  classes->clear();
  classes->reserve(breaks.size() + 1);
  double prev = 0.0;
  for (double b : breaks) {
    StormSurgeDepthClass c;
    c.lo = prev;
    c.hi = b;
    c.unbounded_hi = false;
    classes->push_back(c);
    prev = b;
  }
  {
    StormSurgeDepthClass c;
    c.lo = prev;
    c.hi = 0;
    c.unbounded_hi = true;
    classes->push_back(c);
  }

  const size_t n =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  if (class_mask_out) {
    class_mask_out->assign(n, 0);
  }
  const double cell = storm_surge_cell_area(geotransform);
  for (size_t i = 0; i < n; ++i) {
    if (!mask[i] || !std::isfinite(depth[i]) || depth[i] <= 0.f) {
      continue;
    }
    const double d = static_cast<double>(depth[i]);
    size_t cls = 0;
    for (; cls < breaks.size(); ++cls) {
      if (d < breaks[cls]) {
        break;
      }
    }
    // cls == breaks.size() → unbounded top bin.
    if (cls >= classes->size()) {
      continue;
    }
    (*classes)[cls].cell_count += 1;
    (*classes)[cls].area += cell;
    if (class_mask_out) {
      (*class_mask_out)[i] = static_cast<unsigned char>(cls + 1);
    }
  }
  return true;
}

StormSurgeStatsResult compute_storm_surge_stats(
    const unsigned char* mask,
    const float* depth,
    int width,
    int height,
    const double* geotransform,
    const std::vector<double>& depth_breaks,
    std::string_view impact_path,
    double buffer_distance,
    bool build_class_mask) {
  StormSurgeStatsResult out;
  if (!mask || width <= 0 || height <= 0 || !geotransform) {
    out.error = "bad_mask";
    return out;
  }
  out.width = width;
  out.height = height;
  out.cell_area = storm_surge_cell_area(geotransform);
  if (!compute_inundation_area(mask, width, height, geotransform, &out.wet_cells,
                               &out.inundation_area)) {
    out.error = "area_failed";
    return out;
  }
  if (depth) {
    std::vector<unsigned char>* class_ptr =
        build_class_mask ? &out.class_mask : nullptr;
    if (!classify_storm_surge_depth(depth, mask, width, height, geotransform,
                                    depth_breaks, &out.depth_classes,
                                    class_ptr)) {
      out.error = "classify_failed";
      return out;
    }
  }
  if (!impact_path.empty() || buffer_distance > 0.0) {
    out.overlap = compute_overlap(mask, width, height, geotransform, impact_path,
                                  buffer_distance, "", "");
  }
  out.ok = true;
  return out;
}

bool run_storm_surge_stats_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string mask_path;
  std::string output;
  if (!json_get_string(args, "mask", &mask_path) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  std::string depth_path;
  json_get_string(args, "depth", &depth_path);
  std::string impact_path;
  json_get_string(args, "impact", &impact_path);
  double buffer_distance = 0;
  json_get_double(args, "buffer_distance", &buffer_distance);
  std::string class_mask_output;
  json_get_string(args, "class_mask_output", &class_mask_output);
  std::string buffer_output;
  json_get_string(args, "buffer_output", &buffer_output);
  std::string overlap_output;
  json_get_string(args, "overlap_output", &overlap_output);

  std::vector<unsigned char> mask;
  int width = 0;
  int height = 0;
  double gt[6] = {};
  if (!read_byte_mask(mask_path, &mask, &width, &height, gt)) {
    return false;
  }

  std::vector<float> depth;
  const float* depth_ptr = nullptr;
  if (!depth_path.empty()) {
    if (!read_float_depth(depth_path, width, height, &depth)) {
      return false;
    }
    depth_ptr = depth.data();
  }

  const std::vector<double> breaks = parse_depth_breaks(args);
  const bool want_class = !class_mask_output.empty();

  StormSurgeStatsResult result =
      compute_storm_surge_stats(mask.data(), depth_ptr, width, height, gt,
                                breaks, /*impact_path=*/"",
                                /*buffer_distance=*/0,
                                want_class);
  if (!result.ok) {
    return false;
  }

  // Recompute overlap with optional vector outputs (keeps compute_* free of I/O).
  if (!impact_path.empty() || buffer_distance > 0.0 || !buffer_output.empty() ||
      !overlap_output.empty()) {
    result.overlap =
        compute_overlap(mask.data(), width, height, gt, impact_path,
                        buffer_distance, buffer_output, overlap_output);
    if (!result.overlap.computed && !result.overlap.error.empty() &&
        (!impact_path.empty() || buffer_distance > 0.0)) {
      // Soft-fail overlap into report when polygonize/impact fails.
      result.overlap.computed = false;
    }
  }

  if (want_class) {
    if (result.class_mask.empty() && depth_ptr) {
      classify_storm_surge_depth(depth_ptr, mask.data(), width, height, gt,
                                 breaks, &result.depth_classes,
                                 &result.class_mask);
    }
    if (!result.class_mask.empty() &&
        !write_byte_geotiff(class_mask_output, width, height, gt,
                            result.class_mask.data())) {
      return false;
    }
  }

  write_stats_json(result, output);
  std::ifstream check(output, std::ios::binary);
  return check.good();
}

}  // namespace detail
}  // namespace gis
