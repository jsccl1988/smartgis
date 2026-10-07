// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/ingest/geojson_write.h"

#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace content {
namespace detail {

bool write_geojson_path(const LayerStore& store, const std::string& path) {
  if (path.empty()) {
    return false;
  }
  const GisLayer* layer = store.find_layer(store.active_layer_id());
  if (!layer || !layer->visible || layer->features.empty()) {
    layer = nullptr;
    for (const GisLayer& candidate : store.layers()) {
      if (candidate.visible && !candidate.features.empty()) {
        layer = &candidate;
        break;
      }
    }
  }
  if (!layer || layer->features.empty()) {
    return false;
  }

  const GeomKind dominant = layer->features.front().kind;
  OGRwkbGeometryType wkb = wkbUnknown;
  switch (dominant) {
    case GeomKind::kPoint:
    case GeomKind::kText:
      wkb = wkbPoint;
      break;
    case GeomKind::kLine:
      wkb = wkbLineString;
      break;
    case GeomKind::kPolygon:
      wkb = wkbPolygon;
      break;
  }
  if (wkb == wkbUnknown) {
    return false;
  }

  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  {
    GDALDataset* existing = static_cast<GDALDataset*>(GDALOpenEx(
        path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (existing) {
      GDALClose(existing);
      if (driver->Delete(path.c_str()) != CE_None) {
        DeleteFileA(path.c_str());
      }
    }
  }
  GDALDataset* ds = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!ds) {
    return false;
  }
  OGRLayer* ogr_layer = ds->CreateLayer(
      layer->name.empty() ? "layer" : layer->name.c_str(), nullptr, wkb,
      nullptr);
  if (!ogr_layer) {
    GDALClose(ds);
    return false;
  }

  std::vector<std::string> field_names;
  for (const GisFeature& f : layer->features) {
    if (f.kind != dominant) {
      continue;
    }
    for (const content::NamedField& field : f.fields) {
      if (field.name.empty()) {
        continue;
      }
      bool seen = false;
      for (const std::string& n : field_names) {
        if (n == field.name) {
          seen = true;
          break;
        }
      }
      if (!seen) {
        field_names.push_back(field.name);
      }
    }
  }
  for (const std::string& name : field_names) {
    OGRFieldDefn defn(name.c_str(), OFTString);
    if (ogr_layer->CreateField(&defn) != OGRERR_NONE) {
      GDALClose(ds);
      return false;
    }
  }

  size_t written = 0;
  for (const GisFeature& f : layer->features) {
    if (f.kind != dominant || f.points.empty()) {
      continue;
    }
    OGRFeatureUniquePtr feat(
        OGRFeature::CreateFeature(ogr_layer->GetLayerDefn()));
    if (!feat) {
      continue;
    }
    for (size_t fi = 0; fi < field_names.size(); ++fi) {
      const char* v = named_field_value(f, field_names[fi].c_str());
      if (v) {
        feat->SetField(static_cast<int>(fi), v);
      }
    }

    if (dominant == GeomKind::kPoint || dominant == GeomKind::kText) {
      OGRPoint pt(f.points.front().x, -f.points.front().y);
      feat->SetGeometry(&pt);
    } else if (dominant == GeomKind::kLine) {
      if (f.points.size() < 2) {
        continue;
      }
      OGRLineString line;
      for (const Vertex& p : f.points) {
        line.addPoint(p.x, -p.y);
      }
      feat->SetGeometry(&line);
    } else if (dominant == GeomKind::kPolygon) {
      if (f.points.size() < 3) {
        continue;
      }
      OGRLinearRing ring;
      for (const Vertex& p : f.points) {
        ring.addPoint(p.x, -p.y);
      }
      if (ring.getNumPoints() >= 2) {
        const double x0 = ring.getX(0);
        const double y0 = ring.getY(0);
        const int last = ring.getNumPoints() - 1;
        if (x0 != ring.getX(last) || y0 != ring.getY(last)) {
          ring.addPoint(x0, y0);
        }
      }
      OGRPolygon poly;
      poly.addRing(&ring);
      feat->SetGeometry(&poly);
    }

    if (ogr_layer->CreateFeature(feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }

  GDALClose(ds);
  return written > 0;
}

}  // namespace detail
}  // namespace content
