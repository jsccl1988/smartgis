// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/ir/ir_eval.h"

#include "base/core/log.h"
#include "gis/analysis/ops/ir/ir_text.h"
#include "gis/analysis/ops/ir/ir_visit.h"
#include "gis/analysis/ops/wkt/wkt_codec.h"
#include "gis/feature/feature.h"
#include "gis/geo/ops/geometry_traits.h"
#include "gis/map/map_layer.h"

#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gis {
namespace detail {
namespace {

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

bool open_layer(const std::string& path,
                std::string* error,
                GDALDatasetUniquePtr* ds,
                gis::MapLayer* layer) {
  if (path.empty()) {
    return reject(error, "path");
  }
  GDALAllRegister();
  *ds = GDALDatasetUniquePtr(static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
  if (!*ds || (*ds)->GetLayerCount() < 1) {
    return reject(error, "open");
  }
  *layer = gis::MapLayer::from_ogr((*ds)->GetLayer(0));
  return true;
}

enum class Cmp { kEq, kNe, kLt, kLe, kGt, kGe, kHas, kIn };

// Numeric compare when both sides parse, otherwise lexicographic.
struct EqFn {
  static constexpr Cmp id = Cmp::kEq;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln == rn;
    }
    return left == right;
  }
};

// Not-equal. Numeric when both sides parse, otherwise lexicographic.
struct NeFn {
  static constexpr Cmp id = Cmp::kNe;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln != rn;
    }
    return left != right;
  }
};

// Less-than. Numeric when both sides parse, otherwise lexicographic.
struct LtFn {
  static constexpr Cmp id = Cmp::kLt;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln < rn;
    }
    return left < right;
  }
};

// Less-or-equal. Numeric when both sides parse, otherwise lexicographic.
struct LeFn {
  static constexpr Cmp id = Cmp::kLe;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln <= rn;
    }
    return left <= right;
  }
};

// Greater-than. Numeric when both sides parse, otherwise lexicographic.
struct GtFn {
  static constexpr Cmp id = Cmp::kGt;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln > rn;
    }
    return left > right;
  }
};

// Greater-or-equal. Numeric when both sides parse, otherwise lexicographic.
struct GeFn {
  static constexpr Cmp id = Cmp::kGe;
  static bool apply(std::string_view left, std::string_view right) {
    double ln = 0;
    double rn = 0;
    if (parse_finite(left, &ln) && parse_finite(right, &rn)) {
      return ln >= rn;
    }
    return left >= right;
  }
};

// Substring test. Numbers are not compared, matching the old default arm.
struct HasFn {
  static constexpr Cmp id = Cmp::kHas;
  static bool apply(std::string_view left, std::string_view right) {
    return left.find(right) != std::string_view::npos;
  }
};

// Membership is handled before cmp_values. This arm stays false.
struct InFn {
  static constexpr Cmp id = Cmp::kIn;
  static bool apply(std::string_view, std::string_view) { return false; }
};

using CmpFns = std::tuple<EqFn, NeFn, LtFn, LeFn, GtFn, GeFn, HasFn, InFn>;

// Parsed attribute clause from a filter_features where-text.
struct SimpleFilter {
  enum class Kind { kAll, kNone, kAny, kCmp, kOgr } kind = Kind::kAll;
  Cmp cmp = Cmp::kEq;
  std::string field;
  std::string value;
  std::vector<std::string> options;
};

std::string unquote(std::string_view text) {
  text = trim_ascii(text);
  if (text.size() >= 2 && ((text.front() == '"' && text.back() == '"') ||
                           (text.front() == '\'' && text.back() == '\''))) {
    text.remove_prefix(1);
    text.remove_suffix(1);
  }
  return std::string(text);
}

SimpleFilter parse_filter(std::string_view where) {
  where = trim_ascii(where);
  SimpleFilter filter;
  if (where.empty() || where == "all") {
    return filter;
  }
  if (where == "none") {
    filter.kind = SimpleFilter::Kind::kNone;
    return filter;
  }
  if (where == "any") {
    filter.kind = SimpleFilter::Kind::kAny;
    return filter;
  }
  const std::pair<std::string_view, Cmp> ops[] = {
      {"<=", Cmp::kLe}, {">=", Cmp::kGe}, {"==", Cmp::kEq}, {"!=", Cmp::kNe},
      {"<", Cmp::kLt},  {">", Cmp::kGt},  {" has ", Cmp::kHas}, {" in ", Cmp::kIn},
  };
  for (const auto& item : ops) {
    const std::size_t at = where.find(item.first);
    if (at == std::string_view::npos) {
      continue;
    }
    filter.kind = SimpleFilter::Kind::kCmp;
    filter.cmp = item.second;
    filter.field = unquote(where.substr(0, at));
    const std::string_view rest = where.substr(at + item.first.size());
    if (item.second == Cmp::kIn) {
      std::size_t start = 0;
      const std::string body = unquote(rest);
      while (start <= body.size()) {
        const std::size_t comma = body.find(',', start);
        const std::string piece = unquote(body.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!piece.empty()) {
          filter.options.push_back(piece);
        }
        if (comma == std::string::npos) {
          break;
        }
        start = comma + 1;
      }
    } else {
      filter.value = unquote(rest);
    }
    return filter;
  }
  filter.kind = SimpleFilter::Kind::kOgr;
  filter.value = std::string(where);
  return filter;
}

bool cmp_values(Cmp cmp, std::string_view left, std::string_view right) {
  const auto hit = visit_apply<CmpFns>(cmp, left, right);
  return hit ? *hit : false;
}

bool feature_matches(const gis::Feature& feature, const SimpleFilter& filter) {
  if (filter.kind == SimpleFilter::Kind::kAll || filter.kind == SimpleFilter::Kind::kOgr) {
    return true;
  }
  if (filter.kind == SimpleFilter::Kind::kNone) {
    return false;
  }
  if (filter.kind == SimpleFilter::Kind::kAny) {
    for (int i = 0; i < feature.field_count(); ++i) {
      if (feature.is_field_set(i) && !feature.is_field_null(i)) {
        return true;
      }
    }
    return false;
  }
  const int index = feature.field_index(filter.field.c_str());
  if (index < 0 || feature.is_field_null(index)) {
    return filter.cmp == Cmp::kNe;
  }
  const char* raw = feature.get_field_as_string(index);
  const std::string_view text = raw ? raw : "";
  if (filter.cmp == Cmp::kIn) {
    for (const std::string& option : filter.options) {
      if (text == option) {
        return true;
      }
    }
    return false;
  }
  return cmp_values(filter.cmp, text, filter.value);
}

void append_fields(std::string* out, const gis::Feature& feature, std::string_view only) {
  std::vector<std::string> names;
  if (!only.empty()) {
    std::size_t start = 0;
    while (start <= only.size()) {
      const std::size_t comma = only.find(',', start);
      const std::string piece = unquote(only.substr(
          start, comma == std::string_view::npos ? std::string::npos : comma - start));
      if (!piece.empty()) {
        names.push_back(piece);
      }
      if (comma == std::string_view::npos) {
        break;
      }
      start = comma + 1;
    }
  }
  const OGRFeatureDefn* defn = feature.ogr() ? feature.ogr()->GetDefnRef() : nullptr;
  *out += "\"fields\":{";
  bool first = true;
  const int count = names.empty() ? feature.field_count() : static_cast<int>(names.size());
  for (int i = 0; i < count; ++i) {
    int index = i;
    const char* name = nullptr;
    if (!names.empty()) {
      index = feature.field_index(names[static_cast<std::size_t>(i)].c_str());
      name = names[static_cast<std::size_t>(i)].c_str();
    } else if (defn && defn->GetFieldDefn(i)) {
      name = defn->GetFieldDefn(i)->GetNameRef();
    }
    if (!name) {
      continue;
    }
    if (!first) {
      *out += ',';
    }
    first = false;
    *out += '"';
    *out += json_escape(name);
    *out += "\":";
    if (index < 0 || feature.is_field_null(index) || !feature.is_field_set(index)) {
      *out += "null";
      continue;
    }
    const char* raw = feature.get_field_as_string(index);
    *out += '"';
    *out += json_escape(raw ? raw : "");
    *out += '"';
  }
  *out += '}';
}

std::string feature_json(const gis::Feature& feature, std::string_view fields) {
  std::string out = "{\"fid\":";
  out += std::to_string(feature.id());
  const OGRGeometry* geom = feature.geometry();
  const int dim = geom ? geo::geometry_traits<OGRGeometry>::coordinate_dimension(*geom) : 2;
  out += ",\"dim\":";
  out += std::to_string(dim);
  out += ',';
  append_fields(&out, feature, fields);
  out += ",\"wkt\":\"";
  out += json_escape(geom ? geometry_to_wkt(geom) : "");
  out += "\"}";
  return out;
}

constexpr int kMaxFeatures = 4096;

std::string scan_layer(gis::MapLayer& layer,
                       const SimpleFilter& filter,
                       std::string_view fields,
                       bool* truncated) {
  layer.reset_reading();
  std::string out = "[";
  bool first = true;
  int kept = 0;
  for (;;) {
    gis::Feature feature = layer.next_feature();
    if (!feature.ogr()) {
      break;
    }
    if (!feature_matches(feature, filter)) {
      continue;
    }
    if (kept >= kMaxFeatures) {
      if (truncated) {
        *truncated = true;
      }
      break;
    }
    if (!first) {
      out += ',';
    }
    first = false;
    out += feature_json(feature, fields);
    ++kept;
  }
  out += ']';
  return out;
}

bool scan_filtered(const std::string& path,
                   const SimpleFilter& filter,
                   std::string_view fields,
                   bool apply_ogr_filter,
                   std::string_view spatial_wkt,
                   const char* truncate_verb,
                   std::string* out,
                   std::string* error) {
  GDALDatasetUniquePtr ds;
  gis::MapLayer layer;
  if (!open_layer(path, error, &ds, &layer)) {
    return false;
  }
  if (apply_ogr_filter && filter.kind == SimpleFilter::Kind::kOgr &&
      !layer.set_attribute_filter(filter.value.c_str())) {
    return reject(error, "filter");
  }
  OgrGeom spatial;
  if (!spatial_wkt.empty()) {
    spatial = geometry_from_wkt(spatial_wkt, error);
    if (!spatial) {
      return false;
    }
    layer.set_spatial_filter(spatial.get());
  }
  bool truncated = false;
  const std::string json = scan_layer(layer, filter, fields, &truncated);
  if (truncated && truncate_verb) {
    LOGGING(LOG_WARNING, "%s truncated at %d", truncate_verb, kMaxFeatures);
  }
  return accept(out, json);
}

}  // namespace

bool scan_features(std::string_view path, std::string* out, std::string* error) {
  return scan_filtered(std::string(path), SimpleFilter{}, "", false, "", "scan_features", out,
                       error);
}

bool filter_features(std::string_view path,
                     std::string_view where,
                     std::string_view wkt,
                     std::string* out,
                     std::string* error) {
  return scan_filtered(std::string(path), parse_filter(where), "", true, wkt, "filter_features",
                       out, error);
}

bool read_feature_field(std::string_view path,
                        int32_t fid,
                        std::string_view field,
                        std::string* out,
                        std::string* error) {
  GDALDatasetUniquePtr ds;
  gis::MapLayer layer;
  if (!open_layer(std::string(path), error, &ds, &layer)) {
    return false;
  }
  if (!layer.ogr()) {
    return reject(error, "open");
  }
  const std::string field_name(field);
  gis::Feature feature(layer.ogr()->GetFeature(static_cast<long>(fid)), true);
  if (!feature.ogr()) {
    return reject(error, "feature");
  }
  const int index = feature.field_index(field_name.c_str());
  if (index < 0) {
    return reject(error, "field");
  }
  if (feature.is_field_null(index) || !feature.is_field_set(index)) {
    return accept(out, "");
  }
  const char* raw = feature.get_field_as_string(index);
  return accept(out, raw ? raw : "");
}

bool project_features(std::string_view path,
                      std::string_view fields,
                      std::string* out,
                      std::string* error) {
  return scan_filtered(std::string(path), SimpleFilter{}, fields, false, "", "project_features",
                       out, error);
}

}  // namespace detail
}  // namespace gis
