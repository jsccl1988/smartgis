// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/pointcloud/ingest/io/pdal_io.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointTable.hpp>
#include <pdal/PointView.hpp>
#include <pdal/Stage.hpp>

namespace gis {
namespace {

void fill_from_views(const pdal::PointViewSet& views, size_t max_points,
                     PointCloud* out) {
  out->xyz.clear();
  out->rgba.clear();
  size_t kept = 0;
  for (const pdal::PointViewPtr& view : views) {
    if (!view) {
      continue;
    }
    const bool has_rgb = view->hasDim(pdal::Dimension::Id::Red) &&
                         view->hasDim(pdal::Dimension::Id::Green) &&
                         view->hasDim(pdal::Dimension::Id::Blue);
    for (pdal::PointId i = 0; i < view->size(); ++i) {
      if (max_points != 0 && kept >= max_points) {
        break;
      }
      const double x = view->getFieldAs<double>(pdal::Dimension::Id::X, i);
      const double y = view->getFieldAs<double>(pdal::Dimension::Id::Y, i);
      const double z = view->getFieldAs<double>(pdal::Dimension::Id::Z, i);
      out->xyz.push_back(static_cast<float>(x));
      out->xyz.push_back(static_cast<float>(y));
      out->xyz.push_back(static_cast<float>(z));
      if (has_rgb) {
        const uint16_t r =
            view->getFieldAs<uint16_t>(pdal::Dimension::Id::Red, i);
        const uint16_t g =
            view->getFieldAs<uint16_t>(pdal::Dimension::Id::Green, i);
        const uint16_t b =
            view->getFieldAs<uint16_t>(pdal::Dimension::Id::Blue, i);
        out->rgba.push_back(static_cast<uint8_t>(r > 255 ? r >> 8 : r));
        out->rgba.push_back(static_cast<uint8_t>(g > 255 ? g >> 8 : g));
        out->rgba.push_back(static_cast<uint8_t>(b > 255 ? b >> 8 : b));
        out->rgba.push_back(255);
      }
      ++kept;
    }
    if (max_points != 0 && kept >= max_points) {
      break;
    }
  }
}

bool execute_pipeline_json(const std::string& pipeline_json, size_t max_points,
                           PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (pipeline_json.empty()) {
    out->error = "empty_pipeline";
    return false;
  }
  try {
    pdal::PipelineManager mgr;
    std::istringstream stream(pipeline_json);
    mgr.readPipeline(stream);
    mgr.execute();
    const pdal::PointViewSet& views = mgr.views();
    fill_from_views(views, max_points, out);
  } catch (const pdal::pdal_error& e) {
    out->error = e.what() ? e.what() : "pdal_error";
    return false;
  } catch (const std::exception& e) {
    out->error = e.what() ? e.what() : "pdal_exception";
    return false;
  }
  if (out->empty()) {
    out->error = "no_points";
    return false;
  }
  out->recompute_bounds();
  return true;
}

std::string escape_json_string(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\\' || c == '"') {
      o.push_back('\\');
    }
    o.push_back(c);
  }
  return o;
}

}  // namespace

bool pdal_is_available() {
  return true;
}

bool run_pdal_pipeline_json(const std::string& pipeline_json, PointCloud* out) {
  return execute_pipeline_json(pipeline_json, /*max_points=*/0, out);
}

bool run_pdal_read(const char* path, const PdalReadOptions& options,
                   PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!path || !*path) {
    out->error = "empty_path";
    return false;
  }
  out->source_path = path;

  std::ostringstream json;
  json << "{\"pipeline\":[";
  json << "{\"type\":\"readers.las\",\"filename\":\"" << escape_json_string(path)
       << "\"}";
  if (options.has_z_range) {
    json << ",{\"type\":\"filters.range\",\"limits\":\"Z[" << options.z_min
         << ":" << options.z_max << "]\"}";
  }
  json << "]}";

  const size_t max_pts = options.max_points;
  if (!execute_pipeline_json(json.str(), max_pts, out)) {
    if (out->source_path.empty()) {
      out->source_path = path;
    }
    return false;
  }
  out->source_path = path;
  return true;
}

}  // namespace gis
