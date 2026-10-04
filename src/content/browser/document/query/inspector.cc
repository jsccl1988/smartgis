// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/query/inspector.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "content/public/feature_attrs.h"
#include "gis/carto/style/style_document.h"
#include "gis/carto/style/style_rules.h"

namespace content {
namespace detail {
namespace {

bool source_layer_for_feature(const LayerStore& store,
                              const content::FeatureId& id, std::string* out) {
  if (!out) {
    return false;
  }
  for (const MapLayer& layer : store.layers()) {
    for (const MapFeature& feature : layer.features) {
      if (feature_id_eq(feature.id, id)) {
        *out = layer.name;
        return true;
      }
    }
  }
  return false;
}

bool style_debug_enabled() {
  const char* flag = std::getenv("SMT_FEATURE_INFO_STYLE_DEBUG");
  return flag && flag[0] != '\0' && flag[0] != '0';
}

void append_resolved_paint_rows(
    const gis::style::ResolvedPaint& paint, GeomKind kind,
    std::vector<std::pair<std::string, std::string>>* out) {
  if (!out) {
    return;
  }
  out->push_back({"style layer id", paint.layer_id});
  switch (kind) {
    case GeomKind::kPolygon:
      out->push_back({"fill-color", argb_to_hex_rgb(paint.fill_color)});
      out->push_back({"fill-opacity", std::to_string(paint.fill_opacity)});
      out->push_back(
          {"note", "polygon outline uses fixed GDI admin stroke when fallback"});
      break;
    case GeomKind::kLine:
      out->push_back({"line-color", argb_to_hex_rgb(paint.line_color)});
      out->push_back({"line-width", std::to_string(paint.line_width)});
      out->push_back({"line-opacity", std::to_string(paint.line_opacity)});
      break;
    case GeomKind::kPoint:
      out->push_back({"circle-color", argb_to_hex_rgb(paint.circle_color)});
      out->push_back({"circle-radius", std::to_string(paint.circle_radius)});
      out->push_back(
          {"circle-opacity", std::to_string(paint.circle_opacity)});
      break;
    case GeomKind::kText:
      out->push_back({"text-size", std::to_string(paint.text_size)});
      if (paint.text_halo_width > 0.f) {
        out->push_back(
            {"text-halo-color", argb_to_hex_rgb(paint.text_halo_color)});
        out->push_back(
            {"text-halo-width", std::to_string(paint.text_halo_width)});
      }
      break;
  }
}

void append_style_debug_rows(const LayerStore& store, const StyleBind& style,
                             const MapFeature& f, const std::string& layer_name,
                             double map_scale,
                             std::vector<std::pair<std::string, std::string>>* out) {
  const gis::style::StyleDocument* doc = style.style_document();
  bool using_embedded_carto = false;
  if (!doc) {
    doc = embedded_carto_style_document();
    using_embedded_carto = (doc != nullptr);
  }

  out->push_back({"--- Style (ResolvedPaint) ---", ""});
  if (style.has_style_document()) {
    const std::string label =
        doc && !doc->name.empty() ? doc->name : "(loaded Style JSON)";
    out->push_back({"style source", label});
    out->push_back({"paint resolver", "MapScene StyleDocument"});
  } else if (using_embedded_carto) {
    out->push_back(
        {"style source",
         "(no Style JSON on MapScene  - showing embedded default_carto)"});
    out->push_back(
        {"paint resolver",
         "default_carto_style_json (Map2dPresenter parity; load .style.json "
         "to override)"});
  } else {
    out->push_back({"style source", "(style parse unavailable)"});
  }
  if (!layer_name.empty()) {
    out->push_back({"source-layer", layer_name});
  }
  out->push_back({"zoom", std::to_string(style_zoom_from_scale(map_scale))});

  gis::style::AttrMap attrs;
  for (const content::NamedField& field : f.fields) {
    attrs[field.name] = field.value;
  }
  gis::style::ResolvedPaint paint;
  const bool resolved =
      doc && !layer_name.empty() &&
      gis::style::resolve(*doc, nullptr, attrs, style_zoom_from_scale(map_scale),
                          layer_name, &paint);
  if (resolved) {
    append_resolved_paint_rows(paint, f.kind, out);
  } else if (doc && !layer_name.empty()) {
    out->push_back({"style match", "(no rule for this source-layer / filter)"});
  }

  out->push_back({"--- GDI / legacy render params (debug) ---", ""});
  out->push_back(
      {"note",
       "SmartGis.exe Reg Color table is legacy-only (edit_config_dock_bar); "
       "Style JSON above is the Views primary path."});
  COLORREF fill = 0;
  COLORREF stroke = 0;
  int stroke_w = 0;
  const MapLayer* owner = nullptr;
  for (const MapLayer& layer : store.layers()) {
    for (const MapFeature& candidate : layer.features) {
      if (feature_id_eq(candidate.id, f.id)) {
        owner = &layer;
        break;
      }
    }
    if (owner) {
      break;
    }
  }
  if (owner &&
      style.style_colors_for_feature(*owner, f, map_scale, &fill, &stroke,
                                     &stroke_w)) {
    out->push_back({"gdi fill (style-driven)", colorref_to_hex(fill)});
    out->push_back({"gdi stroke", colorref_to_hex(stroke)});
    out->push_back({"gdi stroke width", std::to_string(stroke_w)});
  } else {
    out->push_back(
        {"gdi brushes",
         "(Baidu defaults in map2d_gdi_paint when Style JSON unset)"});
  }
}

}  // namespace

void fill_feature_info_fields(
    const LayerStore& store, const StyleBind& style, const MapFeature& f,
    std::vector<std::pair<std::string, std::string>>* out,
    const std::string& source_layer, double map_scale) {
  if (!out) {
    return;
  }
  out->clear();

  std::string layer_name = source_layer;
  if (layer_name.empty()) {
    source_layer_for_feature(store, f.id, &layer_name);
  }

  // Product Identify: data attributes + light geometry summary only.
  for (const content::NamedField& field : f.fields) {
    out->push_back({field.name, field.value});
  }
  out->push_back({"geom", f.kind == GeomKind::kLine
                              ? "line"
                              : (f.kind == GeomKind::kPolygon
                                     ? "polygon"
                                     : (f.kind == GeomKind::kText ? "text"
                                                                  : "point"))});
  out->push_back({"vertices", std::to_string(f.points.size())});

  // Style / GDI dumps stay available for harness when explicitly requested.
  if (style_debug_enabled()) {
    append_style_debug_rows(store, style, f, layer_name, map_scale, out);
  }
}

void fill_attribute_rows(const LayerStore& store,
                         std::vector<std::string>* columns,
                         std::vector<std::vector<std::string>>* rows,
                         std::vector<std::string>* tokens) {
  if (!columns || !rows || !tokens) {
    return;
  }
  columns->assign({"FID", "Name", "Type", "Layer"});
  rows->clear();
  tokens->clear();
  for (const MapLayer& layer : store.layers()) {
    for (const MapFeature& f : layer.features) {
      std::string name = f.fields.empty()
                             ? content::encode_feature_token(f.id)
                             : f.fields[0].value;
      std::string type = f.kind == GeomKind::kLine
                             ? "line"
                             : (f.kind == GeomKind::kPolygon
                                    ? "region"
                                    : (f.kind == GeomKind::kText ? "text"
                                                                : "point"));
      for (const content::NamedField& field : f.fields) {
        if (field.name == "kind" || field.name == "type") {
          type = field.value;
        }
        if (field.name == "name" && !field.value.empty()) {
          name = field.value;
        }
        if (field.name == "anno" && !field.value.empty()) {
          name = field.value;
        }
      }
      rows->push_back(
          {content::encode_feature_token(f.id), name, type, layer.name});
      tokens->push_back(content::encode_feature_token(f.id));
    }
  }
}

}  // namespace detail
}  // namespace content
