// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// POD BatchLayer -> owned OGR LayerBatchSet. Scale / role / stem policy
// lives in carto/filter; this TU owns geometry construction.

#include "vista/component/map/batch.h"

#include "vista/component/map/carto/filter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ogrsf_frmts.h"

namespace vista {

namespace {

bool is_carto_slot(const std::string& name) {
  return name == "land" || name == "water" || name == "river" ||
         name == "admin" || name == "road" || name == "label";
}

std::string ascii_lower(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return text;
}

struct FeatureFields {
  const char* kind = nullptr;
  const char* cls = nullptr;
  const char* fclass = nullptr;
  const char* anno = nullptr;
  const char* name = nullptr;
  const char* adcode = nullptr;
  const char* type = nullptr;
};

FeatureFields probe_fields(const BatchFeature& f) {
  FeatureFields out;
  for (const BatchField& field : f.fields) {
    if (!out.kind && field.name == "kind") {
      out.kind = field.value.c_str();
    } else if (!out.cls && field.name == "class") {
      out.cls = field.value.c_str();
    } else if (!out.fclass && field.name == "fclass") {
      out.fclass = field.value.c_str();
    } else if (!out.anno && field.name == "anno") {
      out.anno = field.value.c_str();
    } else if (!out.name && field.name == "name") {
      out.name = field.value.c_str();
    } else if (!out.adcode && field.name == "adcode") {
      out.adcode = field.value.c_str();
    } else if (!out.type && field.name == "type") {
      out.type = field.value.c_str();
    }
  }
  return out;
}

const char* feature_class_cstr(const FeatureFields& fields) {
  if (fields.cls && fields.cls[0]) {
    return fields.cls;
  }
  if (fields.fclass && fields.fclass[0]) {
    return fields.fclass;
  }
  return nullptr;
}

std::string feature_display_name(const FeatureFields& fields) {
  if (fields.anno && fields.anno[0]) {
    return fields.anno;
  }
  if (fields.name && fields.name[0]) {
    return fields.name;
  }
  return {};
}

int feature_label_importance(const FeatureFields& fields) {
  if (fields.cls) {
    if (std::strcmp(fields.cls, "title") == 0) {
      return 3;
    }
    if (std::strcmp(fields.cls, "region_label") == 0) {
      return 2;
    }
    if (std::strcmp(fields.cls, "river_label") == 0) {
      return 1;
    }
  }
  const std::string name = feature_display_name(fields);
  const int by_name = detail::place_name_importance(name.c_str());
  if (by_name >= 3) {
    return 3;
  }
  if (fields.adcode) {
    const size_t n = std::strlen(fields.adcode);
    if (n >= 6) {
      const bool z45 = fields.adcode[4] == '0' && fields.adcode[5] == '0';
      const bool z23 = fields.adcode[2] == '0' && fields.adcode[3] == '0';
      if (z45 && z23) {
        return 3;
      }
      if (z45) {
        return 2;
      }
      if (by_name > 0) {
        return by_name;
      }
      return 1;
    }
  }
  if (by_name > 0) {
    return by_name;
  }
  if (fields.kind) {
    if (std::strcmp(fields.kind, "city") == 0 ||
        std::strcmp(fields.kind, "point") == 0) {
      return 2;
    }
  }
  return 0;
}

std::string carto_source_layer(const BatchFeature& feature,
                               const FeatureFields& fields) {
  std::string kind;
  if (fields.kind && fields.kind[0]) {
    kind = ascii_lower(fields.kind);
  } else if (fields.type && fields.type[0]) {
    kind = ascii_lower(fields.type);
  } else if (fields.cls && fields.cls[0]) {
    kind = ascii_lower(fields.cls);
  } else if (fields.fclass && fields.fclass[0]) {
    kind = ascii_lower(fields.fclass);
  }
  const auto has = [&](const char* token) {
    return kind.find(token) != std::string::npos;
  };
  if (feature.kind == BatchGeomKind::kPolygon) {
    if (has("water") || has("lake") || has("sea") || has("ocean")) {
      return "water";
    }
    return "land";
  }
  if (feature.kind == BatchGeomKind::kLine) {
    if (has("river") || has("stream") || has("canal") || has("lake")) {
      return "river";
    }
    if (has("admin") || has("bound") || has("border")) {
      return "admin";
    }
    if (has("road") || has("highway") || has("motorway") || has("trunk") ||
        has("primary") || has("secondary") || has("street")) {
      return "road";
    }
    // Unlabeled lines often duplicate land rings (china_city). Catch-all
    // "admin" stacked on land→admin synth caused parallel ghost strokes.
    return {};
  }
  return "label";
}

using BatchSourceIndex = std::unordered_map<std::string, size_t>;

LayerBatch* batch_for(LayerBatchSet* out, const std::string& source,
                      BatchSourceIndex* index) {
  if (index) {
    const auto found = index->find(source);
    if (found != index->end()) {
      return &out->batches[found->second];
    }
    const size_t slot = out->batches.size();
    index->emplace(source, slot);
    LayerBatch created;
    created.source_layer = source;
    out->batches.push_back(std::move(created));
    return &out->batches.back();
  }
  for (LayerBatch& batch : out->batches) {
    if (batch.source_layer == source) {
      return &batch;
    }
  }
  LayerBatch batch;
  batch.source_layer = source;
  out->batches.push_back(std::move(batch));
  return &out->batches.back();
}

double polyline_length(const BatchFeature& f) {
  if (f.points.size() < 2) {
    return 0.0;
  }
  double len = 0.0;
  for (size_t i = 1; i < f.points.size(); ++i) {
    len += std::hypot(f.points[i].x - f.points[i - 1].x,
                      f.points[i].y - f.points[i - 1].y);
  }
  return len;
}

int overview_vertex_step(int n, double scale) {
  int step = 1;
  if (n > 16 && scale < 28.0) {
    step = (std::max)(1, n / 40);
  }
  if (n > 24 && scale < 20.0) {
    step = (std::max)(step, n / 44);
  }
  if (n > 32 && scale < 16.0) {
    step = (std::max)(step, n / 48);
  }
  if (n > 48 && scale < 12.0) {
    step = (std::max)(step, n / 32);
  }
  if (n > 64 && scale < 10.0) {
    step = (std::max)(step, n / 24);
  }
  if (n > 128 && scale < 8.0) {
    step = (std::max)(step, n / 18);
  }
  return step;
}

constexpr bool kExtentLonlat = true;

using LineStemLens = std::unordered_map<const BatchFeature*, double>;

struct FeatureProbe {
  const BatchFeature* feature = nullptr;
  FeatureFields fields;
  std::string source;
  detail::LineRole role = detail::LineRole::kOther;
  double len_deg = 0.0;
  bool major = false;
  int label_importance = 0;
  bool stem_water = false;
  bool stem_road = false;
};

bool should_keep_probe(const FeatureProbe& p, double scale, bool use_carto_slots,
                       const LineStemLens* line_stems) {
  if (p.source.empty()) {
    return false;
  }
  if (scale <= 0.0) {
    return true;
  }
  const BatchFeature& f = *p.feature;
  if (!use_carto_slots && (f.kind == BatchGeomKind::kPoint ||
                           f.kind == BatchGeomKind::kText)) {
    return true;
  }
  if (p.source == "label" || f.kind == BatchGeomKind::kText ||
      f.kind == BatchGeomKind::kPoint) {
    int min_imp = detail::label_min_importance(scale);
    if (scale < 12.0 && min_imp < 2) {
      min_imp = 2;
    }
    return p.label_importance >= min_imp;
  }
  if (f.kind == BatchGeomKind::kLine) {
    double len_deg = p.len_deg;
    if ((p.role == detail::LineRole::kWater ||
         p.role == detail::LineRole::kRoad) &&
        line_stems) {
      const auto it = line_stems->find(p.feature);
      if (it != line_stems->end() && it->second > len_deg) {
        len_deg = it->second;
      }
    }
    bool major = p.major;
    if (p.role == detail::LineRole::kRoad && scale < 16.0) {
      auto is_secondary = [](const char* s) {
        if (!s || !s[0]) {
          return false;
        }
        return std::strstr(s, "secondary") != nullptr ||
               std::strstr(s, "Secondary") != nullptr;
      };
      if (is_secondary(p.fields.kind) ||
          is_secondary(feature_class_cstr(p.fields))) {
        major = false;
      }
    }
    return detail::line_visible_at_scale(p.role, len_deg, major, scale);
  }
  return true;
}

void fill_stem_lens_from_entries(
    std::vector<std::pair<const BatchFeature*, std::string>>* named,
    std::vector<detail::StemSpan>* spans, LineStemLens* out) {
  if (!named || !spans || !out || named->empty()) {
    return;
  }
  for (size_t i = 0; i < named->size(); ++i) {
    (*spans)[i].name = (*named)[i].second.c_str();
  }
  std::vector<double> stem_lens(spans->size(), 0.0);
  detail::fill_stem_lengths(spans->data(), spans->size(), 0.05, stem_lens.data());
  for (size_t i = 0; i < named->size(); ++i) {
    out->emplace((*named)[i].first, stem_lens[i]);
  }
}

LineStemLens build_line_stem_lengths(const std::vector<FeatureProbe>& probes) {
  struct StemBucket {
    std::vector<std::pair<const BatchFeature*, std::string>> named;
    std::vector<detail::StemSpan> spans;
  };
  StemBucket water;
  StemBucket road;
  water.named.reserve(probes.size() / 8 + 1);
  water.spans.reserve(probes.size() / 8 + 1);
  road.named.reserve(probes.size() / 4 + 1);
  road.spans.reserve(probes.size() / 4 + 1);
  auto push_entry = [](StemBucket* bucket, const FeatureProbe& p) {
    const BatchFeature& f = *p.feature;
    detail::StemSpan span;
    span.length = p.len_deg;
    span.x0 = f.points.front().x;
    span.y0 = f.points.front().y;
    span.x1 = f.points.back().x;
    span.y1 = f.points.back().y;
    bucket->named.emplace_back(p.feature, feature_display_name(p.fields));
    bucket->spans.push_back(span);
  };
  for (const FeatureProbe& p : probes) {
    if (p.stem_water) {
      push_entry(&water, p);
    } else if (p.stem_road) {
      push_entry(&road, p);
    }
  }
  LineStemLens out;
  fill_stem_lens_from_entries(&water.named, &water.spans, &out);
  fill_stem_lens_from_entries(&road.named, &road.spans, &out);
  return out;
}

FeatureProbe make_probe(const BatchFeature& f, const std::string& layer_slot,
                        bool layer_is_slot, bool use_carto_slots) {
  FeatureProbe p;
  p.feature = &f;
  p.fields = probe_fields(f);
  p.source = !use_carto_slots
                 ? layer_slot
                 : (layer_is_slot ? layer_slot : carto_source_layer(f, p.fields));
  const char* cls = feature_class_cstr(p.fields);
  p.role = detail::line_role(p.fields.kind, cls);
  p.major = detail::line_is_major_class(p.fields.kind, cls);
  if (f.kind == BatchGeomKind::kLine && f.points.size() >= 2) {
    p.len_deg = detail::length_as_degrees(polyline_length(f), kExtentLonlat);
    p.stem_water =
        (p.source == "river") || (p.role == detail::LineRole::kWater);
    p.stem_road = (p.source == "road") || (p.role == detail::LineRole::kRoad);
  }
  if (p.source == "label" || f.kind == BatchGeomKind::kText ||
      f.kind == BatchGeomKind::kPoint) {
    p.label_importance = feature_label_importance(p.fields);
  }
  return p;
}

// Quantize lon/lat so shared provincial borders hash as one undirected edge.
uint64_t quantize_lonlat(double x, double y) {
  const auto qx = static_cast<int32_t>(std::llround(x * 10000.0));
  const auto qy = static_cast<int32_t>(std::llround(y * 10000.0));
  return (static_cast<uint64_t>(static_cast<uint32_t>(qx)) << 32) |
         static_cast<uint32_t>(qy);
}

struct UndirectedEdgeKey {
  uint64_t a = 0;
  uint64_t b = 0;
  bool operator==(const UndirectedEdgeKey& o) const {
    return a == o.a && b == o.b;
  }
};

struct UndirectedEdgeHash {
  size_t operator()(const UndirectedEdgeKey& e) const {
    return static_cast<size_t>(e.a ^ (e.b * 0x9e3779b97f4a7c15ull));
  }
};

using AdminEdgeSet = std::unordered_set<UndirectedEdgeKey, UndirectedEdgeHash>;

UndirectedEdgeKey make_edge_key(double x0, double y0, double x1, double y1) {
  uint64_t a = quantize_lonlat(x0, y0);
  uint64_t b = quantize_lonlat(x1, y1);
  if (a > b) {
    std::swap(a, b);
  }
  return UndirectedEdgeKey{a, b};
}

// Emit land-ring segments as admin once per undirected edge. Adjacent
// admin_1 polygons share borders; stroking every ring twice with independent
// RDP produced parallel ghost strokes on china overview.
void append_unique_admin_edges(const BatchFeature& f, int safe_step,
                               const std::map<std::string, std::string>& attrs,
                               LayerBatchSet* out, BatchSourceIndex* batch_index,
                               AdminEdgeSet* seen_edges) {
  if (!out || !seen_edges || !batch_index) {
    return;
  }
  const int n = static_cast<int>(f.points.size());
  if (n < 3) {
    return;
  }
  std::vector<BatchPoint> ring;
  ring.reserve(static_cast<size_t>(n / (std::max)(safe_step, 1) + 2));
  for (int i = 0; i < n; i += safe_step) {
    ring.push_back(f.points[static_cast<size_t>(i)]);
  }
  if (n > 0 && (n - 1) % safe_step != 0) {
    ring.push_back(f.points.back());
  }
  if (ring.size() >= 2 &&
      (ring.front().x != ring.back().x || ring.front().y != ring.back().y)) {
    ring.push_back(ring.front());
  }
  if (ring.size() < 2) {
    return;
  }
  LayerBatch* admin = batch_for(out, "admin", batch_index);
  auto flush_run = [&](std::unique_ptr<OGRLineString>& run) {
    if (!run || run->getNumPoints() < 2) {
      run.reset();
      return;
    }
    admin->geoms.push_back(run.get());
    out->owned.push_back(std::move(run));
    admin->attrs.push_back(attrs);
  };
  std::unique_ptr<OGRLineString> run;
  for (size_t i = 1; i < ring.size(); ++i) {
    const BatchPoint& p0 = ring[i - 1];
    const BatchPoint& p1 = ring[i];
    const UndirectedEdgeKey key = make_edge_key(p0.x, p0.y, p1.x, p1.y);
    if (key.a == key.b || !seen_edges->insert(key).second) {
      flush_run(run);
      continue;
    }
    if (!run) {
      run = std::make_unique<OGRLineString>();
      run->addPoint(p0.x, p0.y);
    }
    run->addPoint(p1.x, p1.y);
  }
  flush_run(run);
}

void append_probe_geometry(const FeatureProbe& p, double scale,
                           LayerBatchSet* out, BatchSourceIndex* batch_index,
                           AdminEdgeSet* land_admin_edges,
                           bool skip_land_admin_synth) {
  if (!out || !p.feature) {
    return;
  }
  const BatchFeature& f = *p.feature;
  LayerBatch* batch = batch_for(out, p.source, batch_index);
  std::unique_ptr<OGRGeometry> geom;
  const int n = static_cast<int>(f.points.size());
  const int step = (f.kind == BatchGeomKind::kLine ||
                    f.kind == BatchGeomKind::kPolygon)
                       ? overview_vertex_step(n, scale)
                       : 1;
  const int safe_step =
      (f.kind == BatchGeomKind::kPolygon && n / (std::max)(step, 1) < 3)
          ? 1
          : ((f.kind == BatchGeomKind::kLine && n / (std::max)(step, 1) < 2)
                 ? 1
                 : step);
  if (f.kind == BatchGeomKind::kLine) {
    auto line = std::make_unique<OGRLineString>();
    for (int i = 0; i < n; i += safe_step) {
      line->addPoint(f.points[static_cast<size_t>(i)].x,
                     f.points[static_cast<size_t>(i)].y);
    }
    if (n > 0 && (n - 1) % safe_step != 0) {
      line->addPoint(f.points.back().x, f.points.back().y);
    }
    geom = std::move(line);
  } else if (f.kind == BatchGeomKind::kPolygon) {
    auto ring = std::make_unique<OGRLinearRing>();
    for (int i = 0; i < n; i += safe_step) {
      ring->addPoint(f.points[static_cast<size_t>(i)].x,
                     f.points[static_cast<size_t>(i)].y);
    }
    if (n > 0 && (n - 1) % safe_step != 0) {
      ring->addPoint(f.points.back().x, f.points.back().y);
    }
    ring->closeRings();
    auto poly = std::make_unique<OGRPolygon>();
    poly->addRingDirectly(ring.release());
    geom = std::move(poly);
  } else {
    geom = std::make_unique<OGRPoint>(f.points.front().x, f.points.front().y);
  }
  batch->geoms.push_back(geom.get());
  out->owned.push_back(std::move(geom));
  std::map<std::string, std::string> attrs;
  for (const BatchField& field : f.fields) {
    attrs.emplace(field.name, field.value);
  }
  batch->attrs.push_back(attrs);
  // china_city stores admin_1 as land polygons only; river/road extracts
  // have no boundary lines. Stroke unique undirected ring edges as admin.
  if (!skip_land_admin_synth && land_admin_edges && p.source == "land" &&
      f.kind == BatchGeomKind::kPolygon && n >= 3) {
    append_unique_admin_edges(f, safe_step, attrs, out, batch_index,
                              land_admin_edges);
  }
}

void collect_layer_probes(const BatchLayer& layer, bool use_carto_slots,
                          std::vector<FeatureProbe>* probes) {
  if (!probes || !layer.visible) {
    return;
  }
  const std::string layer_slot = layer.name.empty() ? layer.id : layer.name;
  const bool layer_is_slot = use_carto_slots && is_carto_slot(layer_slot);
  probes->reserve(probes->size() + layer.features.size());
  for (const BatchFeature& f : layer.features) {
    if (f.points.empty()) {
      continue;
    }
    probes->push_back(make_probe(f, layer_slot, layer_is_slot, use_carto_slots));
  }
}

void emit_kept_probes(const std::vector<FeatureProbe>& probes, double scale,
                      bool use_carto_slots, const LineStemLens* line_stems,
                      LayerBatchSet* out) {
  if (!out) {
    return;
  }
  // Dedicated admin line features already carry boundaries — do not also
  // synth strokes from every land ring (that doubles shared provincial edges).
  bool has_admin_lines = false;
  for (const FeatureProbe& probe : probes) {
    if (probe.source == "admin" && probe.feature &&
        probe.feature->kind == BatchGeomKind::kLine &&
        should_keep_probe(probe, scale, use_carto_slots, line_stems)) {
      has_admin_lines = true;
      break;
    }
  }
  // Overview / first WaitFirstMapPresent: land-ring → admin edge hashing
  // dominated batches wall; provincial strokes are not readable at this
  // scale (same gate as dedupe_admin_boundary_edges in map2d_batches).
  const bool skip_land_admin_synth = has_admin_lines || scale < 14.0;
  BatchSourceIndex batch_index;
  batch_index.reserve(8);
  AdminEdgeSet land_admin_edges;
  AdminEdgeSet* edges =
      skip_land_admin_synth ? nullptr : &land_admin_edges;
  if (edges) {
    land_admin_edges.reserve(4096);
  }
  for (const FeatureProbe& probe : probes) {
    if (!should_keep_probe(probe, scale, use_carto_slots, line_stems)) {
      continue;
    }
    append_probe_geometry(probe, scale, out, &batch_index, edges,
                          skip_land_admin_synth);
  }
}

// Land→admin ring synth can still stroke one provincial border twice after
// independent RDP. Collapse finished admin LineStrings onto unique edges.
void append_unique_line_edges(
    const OGRLineString* line, AdminEdgeSet* seen, LayerBatch* admin,
    LayerBatchSet* out, const std::map<std::string, std::string>& attrs) {
  if (!line || !seen || !admin || !out) {
    return;
  }
  const int n = line->getNumPoints();
  if (n < 2) {
    return;
  }
  auto flush_run = [&](std::unique_ptr<OGRLineString>& run) {
    if (!run || run->getNumPoints() < 2) {
      run.reset();
      return;
    }
    admin->geoms.push_back(run.get());
    out->owned.push_back(std::move(run));
    admin->attrs.push_back(attrs);
  };
  std::unique_ptr<OGRLineString> run;
  for (int i = 1; i < n; ++i) {
    const double x0 = line->getX(i - 1);
    const double y0 = line->getY(i - 1);
    const double x1 = line->getX(i);
    const double y1 = line->getY(i);
    const UndirectedEdgeKey key = make_edge_key(x0, y0, x1, y1);
    if (key.a == key.b || !seen->insert(key).second) {
      flush_run(run);
      continue;
    }
    if (!run) {
      run = std::make_unique<OGRLineString>();
      run->addPoint(x0, y0);
    }
    run->addPoint(x1, y1);
  }
  flush_run(run);
}

void dedupe_admin_boundary_edges_impl(LayerBatchSet* set) {
  if (!set) {
    return;
  }
  LayerBatch* admin = nullptr;
  for (LayerBatch& batch : set->batches) {
    if (batch.source_layer == "admin") {
      admin = &batch;
      break;
    }
  }
  if (!admin || admin->geoms.size() < 2) {
    return;
  }
  std::vector<const OGRGeometry*> old_geoms = std::move(admin->geoms);
  std::vector<std::map<std::string, std::string>> old_attrs =
      std::move(admin->attrs);
  admin->geoms.clear();
  admin->attrs.clear();
  admin->geoms.reserve(old_geoms.size());
  admin->attrs.reserve(old_attrs.size());
  AdminEdgeSet seen;
  seen.reserve(old_geoms.size() * 64);
  for (size_t i = 0; i < old_geoms.size(); ++i) {
    const OGRGeometry* raw = old_geoms[i];
    if (!raw) {
      continue;
    }
    const std::map<std::string, std::string>& attrs =
        i < old_attrs.size() ? old_attrs[i]
                             : std::map<std::string, std::string>{};
    const OGRwkbGeometryType type = wkbFlatten(raw->getGeometryType());
    if (type == wkbLineString) {
      append_unique_line_edges(static_cast<const OGRLineString*>(raw), &seen,
                               admin, set, attrs);
    } else if (type == wkbMultiLineString) {
      const auto* multi = static_cast<const OGRMultiLineString*>(raw);
      const int n = multi->getNumGeometries();
      for (int g = 0; g < n; ++g) {
        append_unique_line_edges(
            static_cast<const OGRLineString*>(multi->getGeometryRef(g)), &seen,
            admin, set, attrs);
      }
    } else {
      // Keep non-line admin geoms as-is (unexpected but safe).
      admin->geoms.push_back(raw);
      admin->attrs.push_back(attrs);
    }
  }
}

}  // namespace

void dedupe_admin_boundary_edges(LayerBatchSet* set) {
  dedupe_admin_boundary_edges_impl(set);
}

LayerBatchSet::LayerBatchSet() = default;

LayerBatchSet::~LayerBatchSet() = default;

LayerBatchSet::LayerBatchSet(LayerBatchSet&& other) noexcept
    : batches(std::move(other.batches)), owned(std::move(other.owned)) {}

LayerBatchSet& LayerBatchSet::operator=(LayerBatchSet&& other) noexcept {
  batches = std::move(other.batches);
  owned = std::move(other.owned);
  return *this;
}

LayerBatchSet build_layer_batches(const std::vector<BatchLayer>& layers,
                                  bool use_carto_slots, double scale) {
  std::vector<size_t> visible;
  visible.reserve(layers.size());
  for (size_t i = 0; i < layers.size(); ++i) {
    if (layers[i].visible) {
      visible.push_back(i);
    }
  }
  if (visible.empty()) {
    return {};
  }
  std::vector<FeatureProbe> probes;
  size_t feature_hint = 0;
  for (size_t vi : visible) {
    feature_hint += layers[vi].features.size();
  }
  probes.reserve(feature_hint);
  for (size_t vi : visible) {
    collect_layer_probes(layers[vi], use_carto_slots, &probes);
  }
  const LineStemLens line_stems = build_line_stem_lengths(probes);
  LayerBatchSet out;
  out.batches.reserve(8);
  emit_kept_probes(probes, scale, use_carto_slots, &line_stems, &out);
  return out;
}

LayerBatchSet clone_layer_batch_set(const LayerBatchSet& src) {
  LayerBatchSet out;
  out.owned.reserve(src.owned.size());
  std::unordered_map<const OGRGeometry*, OGRGeometry*> remap;
  remap.reserve(src.owned.size());
  for (const std::unique_ptr<OGRGeometry>& g : src.owned) {
    if (!g) {
      out.owned.emplace_back();
      continue;
    }
    OGRGeometry* copy = g->clone();
    remap.emplace(g.get(), copy);
    out.owned.emplace_back(copy);
  }
  out.batches.reserve(src.batches.size());
  for (const LayerBatch& b : src.batches) {
    LayerBatch nb;
    nb.source_layer = b.source_layer;
    nb.attrs = b.attrs;
    nb.geoms.reserve(b.geoms.size());
    for (const OGRGeometry* g : b.geoms) {
      const auto it = remap.find(g);
      nb.geoms.push_back(it == remap.end() ? nullptr : it->second);
    }
    out.batches.push_back(std::move(nb));
  }
  return out;
}

}  // namespace vista
