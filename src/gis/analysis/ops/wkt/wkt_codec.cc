// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/wkt/wkt_codec.h"

#include "gis/analysis/ops/ir/ir_text.h"

#include "cpl_conv.h"
#include "ogr_geometry.h"

#include <string>

namespace gis {
namespace detail {
namespace {

bool is_tin(const OGRGeometry& geom) {
  const OGRwkbGeometryType flat = wkbFlatten(geom.getGeometryType());
  return flat == wkbTIN || flat == wkbPolyhedralSurface;
}

}  // namespace

OgrGeom geometry_from_wkt(std::string_view text, std::string* error) {
  const std::string_view trimmed = trim_ascii(text);
  if (trimmed.empty()) {
    reject(error, "missing geometry");
    return {};
  }
  const std::string owned(trimmed);
  OGRGeometry* raw = nullptr;
  if (OGRGeometryFactory::createFromWkt(owned.c_str(), nullptr, &raw) != OGRERR_NONE || !raw) {
    reject(error, "bad wkt");
    return {};
  }
  if (is_tin(*raw)) {
    OGRGeometryFactory::destroyGeometry(raw);
    reject(error, "tin is not a polygon");
    return {};
  }
  return OgrGeom(raw);
}

std::string geometry_to_wkt(const OGRGeometry* geom) {
  if (!geom) {
    return {};
  }
  char* text = nullptr;
  if (geom->exportToWkt(&text) != OGRERR_NONE || !text) {
    CPLFree(text);
    return {};
  }
  std::string out(text);
  CPLFree(text);
  return out;
}

}  // namespace detail
}  // namespace gis
