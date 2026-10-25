// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/layer/layer_ops.h"

#include "base/core/log.h"
#include "gis/analysis/ops/runner/ops_args.h"
#include "gis/geo/ops/predicate.h"
#include "gis/geo/ops/set.h"
#include "gis/geo/ops/shape.h"

#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gis {
namespace detail {
namespace {

constexpr int kMaxLayerFeatures = 4096;

struct OpenLayer {
  GDALDatasetUniquePtr ds;
  OGRLayer* layer = nullptr;
};

bool open_read_layer(std::string_view path, OpenLayer* out, std::string* error) {
  if (!out || path.empty()) {
    if (error) {
      *error = "path";
    }
    return false;
  }
  GDALAllRegister();
  out->ds = GDALDatasetUniquePtr(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
      nullptr, nullptr)));
  if (!out->ds || out->ds->GetLayerCount() < 1) {
    if (error) {
      *error = "open";
    }
    return false;
  }
  out->layer = out->ds->GetLayer(0);
  return out->layer != nullptr;
}

std::string field_key_string(OGRFeature* feat, int field_index) {
  if (!feat || field_index < 0 || feat->IsFieldNull(field_index) ||
      !feat->IsFieldSet(field_index)) {
    return {};
  }
  const char* raw = feat->GetFieldAsString(field_index);
  return raw ? raw : "";
}

std::vector<std::string> split_field_list(std::string_view text) {
  std::vector<std::string> names;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t comma = text.find(',', start);
    std::string_view piece = text.substr(
        start, comma == std::string_view::npos ? std::string::npos : comma - start);
    while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t')) {
      piece.remove_prefix(1);
    }
    while (!piece.empty() && (piece.back() == ' ' || piece.back() == '\t')) {
      piece.remove_suffix(1);
    }
    if (piece.size() >= 2 &&
        ((piece.front() == '"' && piece.back() == '"') ||
         (piece.front() == '\'' && piece.back() == '\''))) {
      piece.remove_prefix(1);
      piece.remove_suffix(1);
    }
    if (!piece.empty()) {
      names.emplace_back(piece);
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return names;
}

std::unique_ptr<OGRGeometry> union_layer_geometries(OGRLayer* layer, int* feature_count) {
  if (!layer) {
    return nullptr;
  }
  layer->ResetReading();
  OGRGeometryCollection collection;
  int count = 0;
  while (OGRFeature* feat = layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (count >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "layer union truncated at %d", kMaxLayerFeatures);
      break;
    }
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    collection.addGeometry(geom);
    ++count;
  }
  if (feature_count) {
    *feature_count = count;
  }
  if (collection.IsEmpty()) {
    return nullptr;
  }
  return std::unique_ptr<OGRGeometry>(geo::unary_union(collection));
}

OGRLayer* create_output_layer(GDALDataset* ds,
                              const char* layer_name,
                              OGRFeatureDefn* schema) {
  if (!ds || !schema) {
    return nullptr;
  }
  OGRLayer* layer =
      ds->CreateLayer(layer_name, schema->GetGeomFieldDefn(0)->GetSpatialRef(),
                      schema->GetGeomFieldDefn(0)->GetType(), nullptr);
  if (!layer) {
    return nullptr;
  }
  for (int i = 0; i < schema->GetFieldCount(); ++i) {
    OGRFieldDefn* defn = schema->GetFieldDefn(i);
    if (defn) {
      layer->CreateField(defn);
    }
  }
  return layer;
}

bool open_write_geojson(std::string_view path, GDALDatasetUniquePtr* ds) {
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver || path.empty()) {
    return false;
  }
  VSIUnlink(std::string(path).c_str());
  *ds = GDALDatasetUniquePtr(
      driver->Create(std::string(path).c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  return *ds != nullptr;
}

void copy_named_fields(OGRFeature* dst, OGRFeature* src,
                       const std::vector<std::string>& names, const char* name_prefix) {
  if (!dst || !src) {
    return;
  }
  OGRFeatureDefn* src_defn = src->GetDefnRef();
  OGRFeatureDefn* dst_defn = dst->GetDefnRef();
  if (!src_defn || !dst_defn) {
    return;
  }
  for (const std::string& name : names) {
    const int src_index = src->GetFieldIndex(name.c_str());
    if (src_index < 0) {
      continue;
    }
    std::string dst_name = name;
    if (name_prefix && *name_prefix) {
      dst_name = std::string(name_prefix) + name;
    }
    const int dst_index = dst->GetFieldIndex(dst_name.c_str());
    if (dst_index < 0) {
      continue;
    }
    if (src->IsFieldNull(src_index) || !src->IsFieldSet(src_index)) {
      dst->UnsetField(dst_index);
      continue;
    }
    dst->SetField(dst_index, src->GetRawFieldRef(src_index));
  }
}

void copy_all_fields(OGRFeature* dst, OGRFeature* src) {
  if (!dst || !src) {
    return;
  }
  OGRFeatureDefn* defn = src->GetDefnRef();
  if (!defn) {
    return;
  }
  for (int i = 0; i < defn->GetFieldCount(); ++i) {
    if (src->IsFieldNull(i) || !src->IsFieldSet(i)) {
      continue;
    }
    const char* name = defn->GetFieldDefn(i)->GetNameRef();
    const int dst_index = dst->GetFieldIndex(name);
    if (dst_index >= 0) {
      dst->SetField(dst_index, src->GetRawFieldRef(i));
    }
  }
}

std::optional<geo::SpatialPredicate> parse_predicate(std::string_view text) {
  if (text.empty() || text == "intersects") {
    return geo::SpatialPredicate::kIntersects;
  }
  if (text == "contains") {
    return geo::SpatialPredicate::kContains;
  }
  if (text == "within") {
    return geo::SpatialPredicate::kWithin;
  }
  if (text == "touches") {
    return geo::SpatialPredicate::kTouches;
  }
  if (text == "crosses") {
    return geo::SpatialPredicate::kCrosses;
  }
  if (text == "overlaps") {
    return geo::SpatialPredicate::kOverlaps;
  }
  if (text == "covers") {
    return geo::SpatialPredicate::kCovers;
  }
  if (text == "covered_by") {
    return geo::SpatialPredicate::kCoveredBy;
  }
  if (text == "disjoint") {
    return geo::SpatialPredicate::kDisjoint;
  }
  if (text == "equals") {
    return geo::SpatialPredicate::kEquals;
  }
  return std::nullopt;
}

LayerOpResult fail_result(std::string_view code) {
  LayerOpResult r;
  r.error = std::string(code);
  return r;
}

LayerOpResult ok_result(int count) {
  LayerOpResult r;
  r.ok = true;
  r.features_out = count;
  return r;
}

void ensure_prefixed_fields(OGRLayer* out_layer,
                            OGRFeatureDefn* src_defn,
                            const std::vector<std::string>& names,
                            const char* prefix) {
  if (!out_layer || !src_defn || !prefix) {
    return;
  }
  for (const std::string& name : names) {
    const int src_index = src_defn->GetFieldIndex(name.c_str());
    if (src_index < 0) {
      continue;
    }
    OGRFieldDefn* src_field = src_defn->GetFieldDefn(src_index);
    if (!src_field) {
      continue;
    }
    const std::string out_name = std::string(prefix) + name;
    if (out_layer->GetLayerDefn()->GetFieldIndex(out_name.c_str()) >= 0) {
      continue;
    }
    OGRFieldDefn copy_defn(out_name.c_str(), src_field->GetType());
    copy_defn.SetWidth(src_field->GetWidth());
    copy_defn.SetPrecision(src_field->GetPrecision());
    out_layer->CreateField(&copy_defn);
  }
}

bool project_point_onto_line(const OGRLineString& line,
                             double x,
                             double y,
                             double* along,
                             double* dist_to) {
  const int n = line.getNumPoints();
  if (n < 2 || !along || !dist_to) {
    return false;
  }
  double best_along = 0.0;
  double best_dist = 1e300;
  double travelled = 0.0;
  for (int i = 0; i + 1 < n; ++i) {
    const double x0 = line.getX(i);
    const double y0 = line.getY(i);
    const double x1 = line.getX(i + 1);
    const double y1 = line.getY(i + 1);
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double seg_len2 = dx * dx + dy * dy;
    double t = 0.0;
    if (seg_len2 > 0.0) {
      t = ((x - x0) * dx + (y - y0) * dy) / seg_len2;
      if (t < 0.0) {
        t = 0.0;
      } else if (t > 1.0) {
        t = 1.0;
      }
    }
    const double px = x0 + t * dx;
    const double py = y0 + t * dy;
    const double d = std::hypot(x - px, y - py);
    const double seg_len = std::sqrt(seg_len2);
    if (d < best_dist) {
      best_dist = d;
      best_along = travelled + t * seg_len;
    }
    travelled += seg_len;
  }
  *along = best_along;
  *dist_to = best_dist;
  return true;
}

std::unique_ptr<OGRLineString> extract_line_substring(const OGRLineString& line,
                                                      double start,
                                                      double end) {
  const double length = line.get_Length();
  if (length <= 0.0 || end <= start + 1e-12) {
    return nullptr;
  }
  if (start < 0.0) {
    start = 0.0;
  }
  if (end > length) {
    end = length;
  }
  auto out = std::make_unique<OGRLineString>();
  OGRPoint p0;
  line.Value(start, &p0);
  out->addPoint(&p0);
  const int n = line.getNumPoints();
  double travelled = 0.0;
  for (int i = 0; i + 1 < n; ++i) {
    const double x0 = line.getX(i);
    const double y0 = line.getY(i);
    const double x1 = line.getX(i + 1);
    const double y1 = line.getY(i + 1);
    const double seg_len = std::hypot(x1 - x0, y1 - y0);
    const double next = travelled + seg_len;
    if (next > start + 1e-12 && travelled < end - 1e-12) {
      if (travelled > start + 1e-12 && travelled < end - 1e-12) {
        out->addPoint(x0, y0);
      }
      if (next < end - 1e-12 && next > start + 1e-12) {
        out->addPoint(x1, y1);
      }
    }
    travelled = next;
  }
  OGRPoint p1;
  line.Value(end, &p1);
  out->addPoint(&p1);
  if (out->getNumPoints() < 2) {
    return nullptr;
  }
  return out;
}

std::vector<std::unique_ptr<OGRGeometry>> split_one_line_at_points(
    const OGRLineString& line,
    const std::vector<OGRPoint>& points,
    double tolerance) {
  std::vector<std::unique_ptr<OGRGeometry>> parts;
  const double length = line.get_Length();
  if (length <= 0.0) {
    return parts;
  }
  std::vector<double> cuts;
  cuts.push_back(0.0);
  cuts.push_back(length);
  for (const OGRPoint& pt : points) {
    double along = 0.0;
    double dist = 0.0;
    if (!project_point_onto_line(line, pt.getX(), pt.getY(), &along, &dist)) {
      continue;
    }
    if (dist > tolerance) {
      continue;
    }
    if (along > tolerance && along < length - tolerance) {
      cuts.push_back(along);
    }
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end(),
                         [](double a, double b) { return std::fabs(a - b) < 1e-9; }),
             cuts.end());
  for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
    std::unique_ptr<OGRLineString> piece =
        extract_line_substring(line, cuts[i], cuts[i + 1]);
    if (piece && piece->getNumPoints() >= 2 && !piece->IsEmpty()) {
      parts.push_back(std::move(piece));
    }
  }
  return parts;
}

void collect_points(const OGRGeometry* geom, std::vector<OGRPoint>* out) {
  if (!geom || !out) {
    return;
  }
  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());
  if (type == wkbPoint) {
    const auto* pt = geom->toPoint();
    out->emplace_back(pt->getX(), pt->getY(), pt->getZ());
    return;
  }
  if (type == wkbMultiPoint || type == wkbGeometryCollection) {
    const auto* coll = geom->toGeometryCollection();
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      collect_points(coll->getGeometryRef(i), out);
    }
  }
}

void collect_line_parts(const OGRGeometry* geom,
                        std::vector<const OGRLineString*>* out) {
  if (!geom || !out) {
    return;
  }
  const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());
  if (type == wkbLineString) {
    out->push_back(geom->toLineString());
    return;
  }
  if (type == wkbMultiLineString || type == wkbGeometryCollection) {
    const auto* coll = geom->toGeometryCollection();
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      collect_line_parts(coll->getGeometryRef(i), out);
    }
  }
}

bool write_feature_clone(OGRLayer* out_layer,
                         OGRFeature* src,
                         OGRGeometry* geom_owned) {
  if (!out_layer || !src || !geom_owned) {
    delete geom_owned;
    return false;
  }
  OGRFeatureUniquePtr out_feat(OGRFeature::CreateFeature(out_layer->GetLayerDefn()));
  copy_all_fields(out_feat.get(), src);
  out_feat->SetGeometryDirectly(geom_owned);
  return out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE;
}

}  // namespace

LayerOpResult dissolve_by_field(std::string_view input_path,
                                std::string_view field_name,
                                std::string_view output_path) {
  if (field_name.empty()) {
    return fail_result("field");
  }
  OpenLayer input;
  std::string open_err;
  if (!open_read_layer(input_path, &input, &open_err)) {
    return fail_result(open_err);
  }
  const int key_index = input.layer->GetLayerDefn()->GetFieldIndex(
      std::string(field_name).c_str());
  if (key_index < 0) {
    return fail_result("field");
  }

  std::map<std::string, std::vector<std::unique_ptr<OGRGeometry>>> groups;
  input.layer->ResetReading();
  int read = 0;
  while (OGRFeature* feat = input.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "dissolve truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    const std::string key = field_key_string(feat, key_index);
    groups[key].push_back(std::unique_ptr<OGRGeometry>(geom->clone()));
  }

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer = out_ds->CreateLayer(
      "dissolved", input.layer->GetSpatialRef(), wkbUnknown, nullptr);
  if (!out_layer) {
    return fail_result("output");
  }
  OGRFieldDefn key_defn(std::string(field_name).c_str(), OFTString);
  out_layer->CreateField(&key_defn);

  int written = 0;
  for (auto& [key, geoms] : groups) {
    if (geoms.empty()) {
      continue;
    }
    OGRGeometryCollection collection;
    for (const auto& g : geoms) {
      collection.addGeometry(g.get());
    }
    std::unique_ptr<OGRGeometry> merged(geo::unary_union(collection));
    if (!merged || merged->IsEmpty()) {
      continue;
    }
    OGRFeatureDefn* defn = out_layer->GetLayerDefn();
    OGRFeatureUniquePtr out_feat(OGRFeature::CreateFeature(defn));
    out_feat->SetField(0, key.c_str());
    out_feat->SetGeometryDirectly(merged.release());
    if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }
  return ok_result(written);
}

LayerOpResult clip_layer_by_polygons(std::string_view input_path,
                                     std::string_view clip_layer_path,
                                     std::string_view output_path) {
  OpenLayer input;
  OpenLayer clip;
  std::string open_err;
  if (!open_read_layer(input_path, &input, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(clip_layer_path, &clip, &open_err)) {
    return fail_result("clip");
  }

  std::unique_ptr<OGRGeometry> clip_union = union_layer_geometries(clip.layer, nullptr);
  if (!clip_union || clip_union->IsEmpty()) {
    return fail_result("clip");
  }

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "clipped", input.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }

  input.layer->ResetReading();
  int written = 0;
  int read = 0;
  while (OGRFeature* feat = input.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "clip_layer truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    std::unique_ptr<OGRGeometry> part(geo::intersection(*geom, *clip_union));
    if (!part || part->IsEmpty()) {
      continue;
    }
    OGRFeatureDefn* defn = out_layer->GetLayerDefn();
    OGRFeatureUniquePtr out_feat(OGRFeature::CreateFeature(defn));
    copy_all_fields(out_feat.get(), feat);
    out_feat->SetGeometryDirectly(part.release());
    if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }
  return ok_result(written);
}

LayerOpResult spatial_join_layer(std::string_view target_path,
                                 std::string_view join_path,
                                 std::string_view predicate,
                                 std::string_view copy_fields,
                                 std::string_view output_path) {
  const std::optional<geo::SpatialPredicate> pred = parse_predicate(predicate);
  if (!pred) {
    return fail_result("predicate");
  }

  OpenLayer target;
  OpenLayer join;
  std::string open_err;
  if (!open_read_layer(target_path, &target, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(join_path, &join, &open_err)) {
    return fail_result("join");
  }

  struct JoinRow {
    OGRFeatureUniquePtr feat;
  };
  std::vector<JoinRow> join_rows;
  join.layer->ResetReading();
  int join_read = 0;
  while (OGRFeature* feat = join.layer->GetNextFeature()) {
    if (join_read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "spatial_join join side truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++join_read;
    JoinRow row;
    row.feat.reset(feat->Clone());
    if (row.feat && row.feat->GetGeometryRef() && !row.feat->GetGeometryRef()->IsEmpty()) {
      join_rows.push_back(std::move(row));
    }
  }

  const std::vector<std::string> copy_names = split_field_list(copy_fields);

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "spatial_join", target.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }
  for (const std::string& name : copy_names) {
    const int join_index = join.layer->GetLayerDefn()->GetFieldIndex(name.c_str());
    if (join_index < 0) {
      continue;
    }
    OGRFieldDefn* join_defn = join.layer->GetLayerDefn()->GetFieldDefn(join_index);
    if (!join_defn) {
      continue;
    }
    std::string out_name = std::string("join_") + name;
    OGRFieldDefn copy_defn(out_name.c_str(), join_defn->GetType());
    copy_defn.SetWidth(join_defn->GetWidth());
    copy_defn.SetPrecision(join_defn->GetPrecision());
    out_layer->CreateField(&copy_defn);
  }

  target.layer->ResetReading();
  int written = 0;
  int target_read = 0;
  while (OGRFeature* target_feat = target.layer->GetNextFeature()) {
    OGRFeatureUniquePtr target_holder(target_feat);
    if (target_read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "spatial_join target truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++target_read;
    const OGRGeometry* target_geom = target_feat->GetGeometryRef();
    if (!target_geom || target_geom->IsEmpty()) {
      continue;
    }
    const JoinRow* match = nullptr;
    for (const JoinRow& row : join_rows) {
      const OGRGeometry* join_geom = row.feat->GetGeometryRef();
      if (!join_geom) {
        continue;
      }
      if (geo::predicate_holds(*pred, *target_geom, *join_geom)) {
        match = &row;
        break;
      }
    }
    if (!match) {
      continue;
    }
    OGRFeatureDefn* defn = out_layer->GetLayerDefn();
    OGRFeatureUniquePtr out_feat(OGRFeature::CreateFeature(defn));
    copy_all_fields(out_feat.get(), target_feat);
    copy_named_fields(out_feat.get(), match->feat.get(), copy_names, "join_");
    out_feat->SetGeometry(target_geom->clone());
    if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }
  return ok_result(written);
}

LayerOpResult erase_layer(std::string_view input_path,
                          std::string_view erase_layer_path,
                          std::string_view output_path) {
  OpenLayer input;
  OpenLayer erase_src;
  std::string open_err;
  if (!open_read_layer(input_path, &input, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(erase_layer_path, &erase_src, &open_err)) {
    return fail_result("erase");
  }

  std::unique_ptr<OGRGeometry> erase_union = union_layer_geometries(erase_src.layer, nullptr);
  if (!erase_union || erase_union->IsEmpty()) {
    return fail_result("erase");
  }

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "erased", input.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }

  input.layer->ResetReading();
  int written = 0;
  int read = 0;
  while (OGRFeature* feat = input.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "erase truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    std::unique_ptr<OGRGeometry> remain(geo::difference(*geom, *erase_union));
    if (!remain || remain->IsEmpty()) {
      continue;
    }
    OGRFeatureDefn* defn = out_layer->GetLayerDefn();
    OGRFeatureUniquePtr out_feat(OGRFeature::CreateFeature(defn));
    copy_all_fields(out_feat.get(), feat);
    out_feat->SetGeometryDirectly(remain.release());
    if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }
  return ok_result(written);
}

bool run_dissolve_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  std::string field;
  if (!op_arg_string(args, "input", &input) || !op_arg_string(args, "output", &output) ||
      !op_arg_string(args, "field", &field)) {
    return false;
  }
  const LayerOpResult r = dissolve_by_field(input, field, output);
  return r.ok;
}

bool run_clip_layer_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  std::string clip;
  if (!op_arg_string(args, "input", &input) || !op_arg_string(args, "output", &output) ||
      !op_arg_string(args, "clip", &clip)) {
    return false;
  }
  const LayerOpResult r = clip_layer_by_polygons(input, clip, output);
  return r.ok;
}

bool run_spatial_join_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string target;
  std::string join;
  std::string output;
  if (!op_arg_string(args, "target", &target) || !op_arg_string(args, "join", &join) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  std::string predicate = "intersects";
  std::string copy_fields;
  (void)op_arg_string(args, "predicate", &predicate);
  (void)op_arg_string(args, "copy_fields", &copy_fields);
  const LayerOpResult r =
      spatial_join_layer(target, join, predicate, copy_fields, output);
  return r.ok;
}

bool run_erase_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  std::string erase;
  if (!op_arg_string(args, "input", &input) || !op_arg_string(args, "output", &output) ||
      !op_arg_string(args, "erase", &erase)) {
    return false;
  }
  const LayerOpResult r = erase_layer(input, erase, output);
  return r.ok;
}

LayerOpResult identity_layer(std::string_view input_path,
                             std::string_view identity_path,
                             std::string_view copy_fields,
                             std::string_view output_path) {
  OpenLayer input;
  OpenLayer identity;
  std::string open_err;
  if (!open_read_layer(input_path, &input, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(identity_path, &identity, &open_err)) {
    return fail_result("identity");
  }

  struct IdRow {
    OGRFeatureUniquePtr feat;
  };
  std::vector<IdRow> id_rows;
  identity.layer->ResetReading();
  int id_read = 0;
  while (OGRFeature* feat = identity.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (id_read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "identity side truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++id_read;
    IdRow row;
    row.feat.reset(feat->Clone());
    if (row.feat && row.feat->GetGeometryRef() &&
        !row.feat->GetGeometryRef()->IsEmpty()) {
      id_rows.push_back(std::move(row));
    }
  }
  if (id_rows.empty()) {
    return fail_result("identity");
  }

  std::unique_ptr<OGRGeometry> id_union =
      union_layer_geometries(identity.layer, nullptr);
  if (!id_union || id_union->IsEmpty()) {
    return fail_result("identity");
  }

  const std::vector<std::string> copy_names = split_field_list(copy_fields);

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "identity", input.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }
  ensure_prefixed_fields(out_layer, identity.layer->GetLayerDefn(), copy_names,
                         "id_");

  input.layer->ResetReading();
  int written = 0;
  int read = 0;
  while (OGRFeature* feat = input.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "identity input truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    for (const IdRow& row : id_rows) {
      const OGRGeometry* id_geom = row.feat->GetGeometryRef();
      if (!id_geom) {
        continue;
      }
      std::unique_ptr<OGRGeometry> overlap(geo::intersection(*geom, *id_geom));
      if (!overlap || overlap->IsEmpty()) {
        continue;
      }
      OGRFeatureUniquePtr out_feat(
          OGRFeature::CreateFeature(out_layer->GetLayerDefn()));
      copy_all_fields(out_feat.get(), feat);
      copy_named_fields(out_feat.get(), row.feat.get(), copy_names, "id_");
      out_feat->SetGeometryDirectly(overlap.release());
      if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
        ++written;
      }
    }
    std::unique_ptr<OGRGeometry> outside(geo::difference(*geom, *id_union));
    if (outside && !outside->IsEmpty()) {
      OGRFeatureUniquePtr out_feat(
          OGRFeature::CreateFeature(out_layer->GetLayerDefn()));
      copy_all_fields(out_feat.get(), feat);
      out_feat->SetGeometryDirectly(outside.release());
      if (out_layer->CreateFeature(out_feat.get()) == OGRERR_NONE) {
        ++written;
      }
    }
  }
  return ok_result(written);
}

LayerOpResult split_line_by_points(std::string_view line_path,
                                   std::string_view point_path,
                                   double tolerance,
                                   std::string_view output_path) {
  if (!(tolerance > 0.0)) {
    tolerance = 1e-6;
  }
  OpenLayer lines;
  OpenLayer points;
  std::string open_err;
  if (!open_read_layer(line_path, &lines, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(point_path, &points, &open_err)) {
    return fail_result("points");
  }

  std::vector<OGRPoint> split_pts;
  points.layer->ResetReading();
  int point_read = 0;
  while (OGRFeature* feat = points.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (point_read >= kMaxLayerFeatures) {
      break;
    }
    ++point_read;
    collect_points(feat->GetGeometryRef(), &split_pts);
  }
  if (split_pts.empty()) {
    return fail_result("points");
  }

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "split_line", lines.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }

  lines.layer->ResetReading();
  int written = 0;
  int read = 0;
  while (OGRFeature* feat = lines.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "split_line truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    std::vector<const OGRLineString*> parts;
    collect_line_parts(feat->GetGeometryRef(), &parts);
    if (parts.empty()) {
      continue;
    }
    bool any = false;
    for (const OGRLineString* part : parts) {
      if (!part) {
        continue;
      }
      std::vector<std::unique_ptr<OGRGeometry>> pieces =
          split_one_line_at_points(*part, split_pts, tolerance);
      if (pieces.empty()) {
        continue;
      }
      any = true;
      for (auto& piece : pieces) {
        if (write_feature_clone(out_layer, feat, piece.release())) {
          ++written;
        }
      }
    }
    if (!any && feat->GetGeometryRef()) {
      if (write_feature_clone(out_layer, feat, feat->GetGeometryRef()->clone())) {
        ++written;
      }
    }
  }
  return ok_result(written);
}

LayerOpResult cut_polygon_by_lines(std::string_view polygon_path,
                                   std::string_view line_path,
                                   std::string_view output_path) {
  OpenLayer polys;
  OpenLayer cutters;
  std::string open_err;
  if (!open_read_layer(polygon_path, &polys, &open_err)) {
    return fail_result(open_err);
  }
  if (!open_read_layer(line_path, &cutters, &open_err)) {
    return fail_result("lines");
  }

  OGRGeometryCollection cutter_edges;
  cutters.layer->ResetReading();
  int cut_read = 0;
  while (OGRFeature* feat = cutters.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (cut_read >= kMaxLayerFeatures) {
      break;
    }
    ++cut_read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    cutter_edges.addGeometry(geom);
  }
  if (cutter_edges.IsEmpty()) {
    return fail_result("lines");
  }

  GDALDatasetUniquePtr out_ds;
  if (!open_write_geojson(output_path, &out_ds)) {
    return fail_result("output");
  }
  OGRLayer* out_layer =
      create_output_layer(out_ds.get(), "cut_polygon", polys.layer->GetLayerDefn());
  if (!out_layer) {
    return fail_result("output");
  }

  polys.layer->ResetReading();
  int written = 0;
  int read = 0;
  while (OGRFeature* feat = polys.layer->GetNextFeature()) {
    OGRFeatureUniquePtr holder(feat);
    if (read >= kMaxLayerFeatures) {
      LOGGING(LOG_WARNING, "cut_polygon truncated at %d", kMaxLayerFeatures);
      break;
    }
    ++read;
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom || geom->IsEmpty()) {
      continue;
    }
    const OGRwkbGeometryType gtype = wkbFlatten(geom->getGeometryType());
    if (gtype != wkbPolygon && gtype != wkbMultiPolygon) {
      continue;
    }
    std::unique_ptr<OGRGeometry> boundary(geo::boundary(*geom));
    if (!boundary || boundary->IsEmpty()) {
      continue;
    }
    OGRGeometryCollection edges;
    edges.addGeometry(boundary.get());
    for (int i = 0; i < cutter_edges.getNumGeometries(); ++i) {
      edges.addGeometry(cutter_edges.getGeometryRef(i));
    }
    std::unique_ptr<OGRGeometry> noded(geo::node(edges));
    if (!noded || noded->IsEmpty()) {
      continue;
    }
    std::unique_ptr<OGRGeometry> pieces(geo::polygonize(*noded));
    if (!pieces || pieces->IsEmpty()) {
      if (write_feature_clone(out_layer, feat, geom->clone())) {
        ++written;
      }
      continue;
    }
    const auto emit_piece = [&](OGRGeometry* piece) {
      if (!piece || piece->IsEmpty()) {
        return;
      }
      if (wkbFlatten(piece->getGeometryType()) != wkbPolygon) {
        return;
      }
      std::unique_ptr<OGRGeometry> probe(geo::point_on_surface(*piece));
      if (!probe || !geo::within(*probe, *geom)) {
        return;
      }
      if (write_feature_clone(out_layer, feat, piece->clone())) {
        ++written;
      }
    };
    if (wkbFlatten(pieces->getGeometryType()) == wkbPolygon) {
      emit_piece(pieces.get());
    } else if (pieces->toGeometryCollection()) {
      OGRGeometryCollection* coll = pieces->toGeometryCollection();
      for (int i = 0; i < coll->getNumGeometries(); ++i) {
        emit_piece(coll->getGeometryRef(i));
      }
    }
  }
  return ok_result(written);
}

bool run_identity_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string identity;
  std::string output;
  if (!op_arg_string(args, "input", &input) ||
      !op_arg_string(args, "identity", &identity) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  std::string copy_fields;
  (void)op_arg_string(args, "copy_fields", &copy_fields);
  const LayerOpResult r = identity_layer(input, identity, copy_fields, output);
  return r.ok;
}

bool run_split_line_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string points;
  std::string output;
  if (!op_arg_string(args, "input", &input) ||
      !op_arg_string(args, "points", &points) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  double tolerance = 1e-6;
  (void)op_arg_double(args, "tolerance", &tolerance);
  const LayerOpResult r = split_line_by_points(input, points, tolerance, output);
  return r.ok;
}

bool run_cut_polygon_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string lines;
  std::string output;
  if (!op_arg_string(args, "input", &input) ||
      !op_arg_string(args, "lines", &lines) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  const LayerOpResult r = cut_polygon_by_lines(input, lines, output);
  return r.ok;
}

}  // namespace detail
}  // namespace gis
