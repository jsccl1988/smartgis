// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/query/inspector.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/public/types.h"
#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"
#include "base/core/log.h"
#include "base/process/switches.h"

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
  const char* flag = base::switch_cstr("feature-info-style-debug");
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
    out->push_back({"paint resolver", "GisScene StyleDocument"});
  } else if (using_embedded_carto) {
    out->push_back(
        {"style source",
         "(no Style JSON on GisScene  - showing embedded default_carto)"});
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
         "(Baidu defaults in map2d hdc paint when Style JSON unset)"});
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

namespace {

// Separate TU-local body so MSVC __try can wrap without C++ dtors in-frame.
void fill_attribute_rows_body(const LayerStore& store,
                              std::vector<std::string>* columns,
                              std::vector<std::vector<std::string>>* rows,
                              std::vector<std::string>* tokens) {
  columns->assign({"FID", "Name", "Type", "Layer"});
  rows->clear();
  tokens->clear();
  constexpr size_t kMaxLayers = 1u << 16;
  constexpr size_t kMaxFeaturesPerLayer = 1u << 20;
  const std::vector<MapLayer>& layers = store.layers();
  const size_t layer_n = layers.size();
  if (layer_n == 0 || layer_n > kMaxLayers || layers.data() == nullptr) {
    LOGGING(LOG_INFO,
            "[fill_attribute_rows] empty_or_invalid layer_n=%zu data=%p",
            layer_n, static_cast<const void*>(layers.data()));
    return;
  }
  for (size_t li = 0; li < layer_n; ++li) {
    const MapLayer& layer = layers[li];
    // Near-null MapLayer* (ABI skew / half-link) shows up as features this≈0x60.
    const uintptr_t layer_addr = reinterpret_cast<uintptr_t>(&layer);
    if (layer_addr < 0x1000u) {
      LOGGING(LOG_ERROR,
              "[fill_attribute_rows] skip corrupt layer ptr=%p li=%zu n=%zu",
              static_cast<const void*>(&layer), li, layer_n);
      continue;
    }
    const size_t feat_n = layer.features.size();
    if (feat_n == 0 || feat_n > kMaxFeaturesPerLayer ||
        layer.features.data() == nullptr) {
      continue;
    }
    for (size_t fi = 0; fi < feat_n; ++fi) {
      const MapFeature& f = layer.features[fi];
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

#if defined(_MSC_VER)
int fill_attribute_rows_seh(const LayerStore& store,
                            std::vector<std::string>* columns,
                            std::vector<std::vector<std::string>>* rows,
                            std::vector<std::string>* tokens) {
  __try {
    fill_attribute_rows_body(store, columns, rows, tokens);
    return 1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}
#endif

}  // namespace

void fill_attribute_rows(const LayerStore& store,
                         std::vector<std::string>* columns,
                         std::vector<std::vector<std::string>>* rows,
                         std::vector<std::string>* tokens) {
  if (!columns || !rows || !tokens) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
#if defined(_MSC_VER)
  // Defense: store can still be mid-destroy from a prior race; mutex stops
  // new races. Soft-fail the table rather than take down the shell.
  if (!fill_attribute_rows_seh(store, columns, rows, tokens)) {
    LOGGING(LOG_ERROR,
            "[fill_attribute_rows] SEH soft-fail (AV during table fill); "
            "returning empty table. Check ABI closure / LayerStore races.");
    columns->assign({"FID", "Name", "Type", "Layer"});
    rows->clear();
    tokens->clear();
  } else {
    LOGGING(LOG_INFO, "[fill_attribute_rows] ok rows=%zu", rows->size());
  }
#else
  fill_attribute_rows_body(store, columns, rows, tokens);
#endif
}

}  // namespace detail
}  // namespace content
