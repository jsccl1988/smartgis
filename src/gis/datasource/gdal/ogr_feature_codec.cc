// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_feature_codec.h"

#include "algorithm/geo/geometry.h"
#include "algorithm/geo/matrix2d.h"
#include "base/carto/style.h"
#include "gis/datasource/gdal/ogr_feature_kind.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace datasource {

namespace {

bool field_nonempty(OGRFeature* src, const char* name) {
  if (!src || !name) {
    return false;
  }
  const int i = src->GetFieldIndex(name);
  if (i < 0) {
    return false;
  }
  const char* v = src->GetFieldAsString(i);
  return v && v[0];
}

bool parse_html_rgb(const char* s, COLORREF* out) {
  if (!s || s[0] != '#' || !out) {
    return false;
  }
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
  if (std::sscanf(s, "#%2x%2x%2x", &r, &g, &b) != 3) {
    return false;
  }
  *out = RGB(r, g, b);
  return true;
}

COLORREF field_color_rgb(OGRFeature* src, const char* name, COLORREF fallback) {
  if (!src || !name) {
    return fallback;
  }
  const int i = src->GetFieldIndex(name);
  if (i < 0) {
    return fallback;
  }
  COLORREF c = fallback;
  if (parse_html_rgb(src->GetFieldAsString(i), &c)) {
    return c;
  }
  return fallback;
}

COLORREF hash_feature_fill(OGRFeature* src, COLORREF fallback) {
  if (!src) {
    return fallback;
  }
  const char* key = nullptr;
  const int ni = src->GetFieldIndex("name");
  if (ni >= 0) {
    key = src->GetFieldAsString(ni);
  }
  if (!key || !key[0]) {
    const int ai = src->GetFieldIndex("adcode");
    if (ai >= 0) {
      key = src->GetFieldAsString(ai);
    }
  }
  if (!key || !key[0]) {
    return fallback;
  }
  unsigned h = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(key); *p;
       ++p) {
    h ^= *p;
    h *= 16777619u;
  }
  // Quiet pastels so fills stay behind labels (Baidu-like wash).
  const int r = 188 + static_cast<int>(h & 0x2fu);
  const int g = 188 + static_cast<int>((h >> 8) & 0x2fu);
  const int b = 188 + static_cast<int>((h >> 16) & 0x2fu);
  return RGB(r, g, b);
}

OGRPoint* first_point(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* pt = dynamic_cast<OGRPoint*>(geom)) {
    return pt;
  }
  if (auto* multi = dynamic_cast<OGRMultiPoint*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRPoint*>(multi->getGeometryRef(0));
    }
  }
  return nullptr;
}

OGRLineString* first_linestring(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* line = dynamic_cast<OGRLineString*>(geom)) {
    return line;
  }
  if (auto* multi = dynamic_cast<OGRMultiLineString*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRLineString*>(multi->getGeometryRef(0));
    }
  }
  return nullptr;
}

OGRPolygon* first_polygon(OGRGeometry* geom) {
  if (!geom) {
    return nullptr;
  }
  if (auto* poly = dynamic_cast<OGRPolygon*>(geom)) {
    return poly;
  }
  if (auto* multi = dynamic_cast<OGRMultiPolygon*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return dynamic_cast<OGRPolygon*>(multi->getGeometryRef(0));
    }
  }
  return nullptr;
}

bool encode_point(const OGRGeometry* src, OGRFeature* dst) {
  OGRPoint* pt = first_point(const_cast<OGRGeometry*>(src));
  if (!pt) {
    return false;
  }
  return dst->SetGeometry(pt) == OGRERR_NONE;
}

OGRGeometry* decode_point(OGRFeature* src) {
  OGRPoint* po_point = first_point(src->GetGeometryRef());
  return po_point ? po_point->clone() : nullptr;
}

bool encode_linestring(const OGRGeometry* src, OGRFeature* dst) {
  OGRLineString* line = first_linestring(const_cast<OGRGeometry*>(src));
  if (!line) {
    return false;
  }
  return dst->SetGeometry(line) == OGRERR_NONE;
}

OGRGeometry* decode_linestring(OGRFeature* src) {
  OGRGeometry* geom = src ? src->GetGeometryRef() : nullptr;
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbLineString || wt == wkbMultiLineString) {
    return geom->clone();
  }
  OGRLineString* po_line = first_linestring(geom);
  return po_line ? po_line->clone() : nullptr;
}

bool encode_polygon(const OGRGeometry* src, OGRFeature* dst) {
  OGRPolygon* poly = first_polygon(const_cast<OGRGeometry*>(src));
  if (!poly) {
    return false;
  }
  return dst->SetGeometry(poly) == OGRERR_NONE;
}

OGRGeometry* decode_polygon(OGRFeature* src) {
  OGRGeometry* geom = src ? src->GetGeometryRef() : nullptr;
  if (!geom) {
    return nullptr;
  }
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (wt == wkbPolygon || wt == wkbMultiPolygon) {
    return geom->clone();
  }
  OGRPolygon* po_poly = first_polygon(geom);
  return po_poly ? po_poly->clone() : nullptr;
}

}  // namespace

gis::SmtFeatureType infer_feature_type(OGRFeature* src,
                                       gis::SmtFeatureType hint) {
  if (hint == gis::SmtFtTin || hint == gis::SmtFtGrid ||
      hint == gis::SmtFtAnno || hint == gis::SmtFtDot ||
      hint == gis::SmtFtCurve || hint == gis::SmtFtSurface) {
    return hint;
  }
  if (!src) {
    return gis::SmtFtUnknown;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return gis::SmtFtUnknown;
  }
  switch (wkbFlatten(geom->getGeometryType())) {
    case wkbPoint:
      // Mixed GeoJSON shares an "anno" column; only nonempty text is a label.
      return field_nonempty(src, "anno") ? gis::SmtFtAnno : gis::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return gis::SmtFtCurve;
    case wkbPolygon:
    case wkbMultiPolygon:
      return src->GetFieldIndex("tin") >= 0 ? gis::SmtFtTin : gis::SmtFtSurface;
    case wkbTIN:
    case wkbTriangle:
      return gis::SmtFtTin;
    case wkbMultiPoint:
      return src->GetFieldIndex("grid_row") >= 0 ? gis::SmtFtGrid
                                                 : gis::SmtFtDot;
    default:
      return gis::SmtFtUnknown;
  }
}

void copy_smt_style_to_ogr(const base::SmtStyle* src, OGRFeature* dst) {
  if (!src || !dst) {
    return;
  }
  const int oi = dst->GetFieldIndex("style");
  if (oi < 0) {
    return;
  }
  dst->SetField(oi, static_cast<int>(sizeof(base::SmtStyle)),
                reinterpret_cast<const void*>(src));
}

base::SmtStyle* copy_ogr_style_from_ogr(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  const int oi = src->GetFieldIndex("style");
  if (oi < 0) {
    return nullptr;
  }
  int nbytes = 0;
  GByte* data = src->GetFieldAsBinary(oi, &nbytes);
  if (!data || nbytes < static_cast<int>(sizeof(base::SmtStyle))) {
    return nullptr;
  }
  auto* style = new base::SmtStyle();
  std::memcpy(style, data, sizeof(base::SmtStyle));
  return style;
}

void fill_default_draw_style(OGRFeature* src, base::SmtStyle* dst, float fblc) {
  if (!dst) {
    return;
  }
  const char* kind = nullptr;
  if (src) {
    const int ki = src->GetFieldIndex("kind");
    if (ki >= 0) {
      kind = src->GetFieldAsString(ki);
    }
  }
  const bool river = kind && (std::strcmp(kind, "river") == 0 ||
                              std::strcmp(kind, "water") == 0);
  base::SmtPenDesc pen;
  pen.lPenStyle = PS_SOLID;
  if (river) {
    pen.lPenColor = field_color_rgb(src, "stroke", RGB(120, 168, 204));
    pen.fPenWidth = fblc > 0.01f ? (0.9f / fblc) : 0.14f;
  } else {
    pen.lPenColor = field_color_rgb(src, "stroke", RGB(78, 92, 108));
    pen.fPenWidth = fblc > 0.01f ? (1.15f / fblc) : 0.2f;
  }
  base::SmtBrushDesc brush;
  COLORREF fill = RGB(214, 226, 236);
  const int fi = src ? src->GetFieldIndex("fill") : -1;
  if (fi < 0 || !parse_html_rgb(src->GetFieldAsString(fi), &fill)) {
    fill = hash_feature_fill(src, fill);
  }
  const int r = (GetRValue(fill) * 55 + 255 * 45) / 100;
  const int g = (GetGValue(fill) * 55 + 255 * 45) / 100;
  const int b = (GetBValue(fill) * 55 + 255 * 45) / 100;
  brush.lBrushColor = river ? RGB(232, 242, 250) : RGB(r, g, b);
  dst->set_pen_desc(pen);
  dst->set_brush_desc(brush);

  unsigned flags = base::ST_PenDesc | base::ST_BrushDesc;
  if (infer_feature_type(src, gis::SmtFtUnknown) == gis::SmtFtAnno) {
    base::SmtAnnotationDesc anno;
    std::strcpy(anno.szFaceName, "Microsoft YaHei");
    anno.lCharSet = DEFAULT_CHARSET;
    anno.lWeight = FW_NORMAL;
    anno.lAnnoClr = field_color_rgb(src, "stroke", RGB(24, 24, 24));
    if (fblc > 0.05f) {
      const float px = anno.fHeight * fblc;
      if (px < 12.f) {
        anno.fHeight = 14.f / fblc;
        anno.fWidth = 0.f;
      } else if (px > 28.f) {
        anno.fHeight = 16.f / fblc;
        anno.fWidth = 0.f;
      }
    }
    dst->set_anno_desc(anno);
    flags |= base::ST_AnnoDesc;
  }
  dst->set_style_type(flags);
}

bool copy_ogr_feature_to_feature(OGRFeature* src, gis::SmtFeature* dst) {
  if (!src || !dst) {
    return false;
  }
  OGRFeature* clone = src->Clone();
  if (!clone) {
    return false;
  }
  dst->reset_ogr(clone, true);

  const gis::SmtFeatureType ft = infer_feature_type(src, gis::SmtFtUnknown);
  dst->set_feature_type(ft);

  if (ft == gis::SmtFtGrid) {
    geo::Grid* grid = decode_smt_grid(src);
    if (!grid) {
      return false;
    }
    dst->set_grid(grid, true);
  } else if (ft == gis::SmtFtTin) {
    geo::Tin* tin = decode_smt_tin(src);
    if (!tin) {
      return false;
    }
    dst->set_tin(tin, true);
  }

  if (base::SmtStyle* sty = copy_ogr_style_from_ogr(src)) {
    dst->set_style(sty);
  }
  return true;
}

bool feature_kind_traits<gis::SmtFtDot>::encode_geom(const OGRGeometry* src,
                                                     OGRFeature* dst) {
  return encode_point(src, dst);
}

OGRGeometry* feature_kind_traits<gis::SmtFtDot>::decode_geom(OGRFeature* src) {
  return decode_point(src);
}

bool feature_kind_traits<gis::SmtFtCurve>::encode_geom(const OGRGeometry* src,
                                                       OGRFeature* dst) {
  return encode_linestring(src, dst);
}

OGRGeometry* feature_kind_traits<gis::SmtFtCurve>::decode_geom(
    OGRFeature* src) {
  return decode_linestring(src);
}

bool feature_kind_traits<gis::SmtFtSurface>::encode_geom(const OGRGeometry* src,
                                                         OGRFeature* dst) {
  return encode_polygon(src, dst);
}

OGRGeometry* feature_kind_traits<gis::SmtFtSurface>::decode_geom(
    OGRFeature* src) {
  return decode_polygon(src);
}

bool feature_kind_traits<gis::SmtFtAnno>::encode_geom(const OGRGeometry* src,
                                                      OGRFeature* dst) {
  return encode_point(src, dst);
}

OGRGeometry* feature_kind_traits<gis::SmtFtAnno>::decode_geom(OGRFeature* src) {
  return decode_point(src);
}

bool encode_tin_mesh(const geo::Tin* tin, OGRFeature* dst) {
  if (!tin || !dst) {
    return false;
  }
  OGRMultiPolygon mp;
  const int n = tin->get_triangle_count();
  for (int i = 0; i < n; ++i) {
    const base::SmtTriangle tri = tin->get_triangle(i);
    if (tri.bDelete) {
      continue;
    }
    const OGRPoint a = tin->get_point(tri.a);
    const OGRPoint b = tin->get_point(tri.b);
    const OGRPoint c = tin->get_point(tri.c);
    // Four unique vertices so GDAL does not promote the part to Triangle/TIN
    // (GPKG's non-standard TIN writer aborts in debug down_cast).
    OGRLinearRing ring;
    ring.addPoint(a.getX(), a.getY(), a.getZ());
    ring.addPoint(b.getX(), b.getY(), b.getZ());
    ring.addPoint(c.getX(), c.getY(), c.getZ());
    ring.addPoint((a.getX() + b.getX() + c.getX()) / 3.0,
                  (a.getY() + b.getY() + c.getY()) / 3.0,
                  (a.getZ() + b.getZ() + c.getZ()) / 3.0);
    ring.closeRings();
    OGRPolygon poly;
    poly.addRing(&ring);
    if (mp.addGeometry(&poly) != OGRERR_NONE) {
      return false;
    }
  }
  const int ti = dst->GetFieldIndex("tin");
  if (ti >= 0) {
    dst->SetField(ti, 1);
  }
  return dst->SetGeometry(&mp) == OGRERR_NONE;
}

bool feature_kind_traits<gis::SmtFtTin>::encode_geom(const OGRGeometry* src,
                                                     OGRFeature* dst) {
  if (!src || !dst) {
    return false;
  }
  const int ti = dst->GetFieldIndex("tin");
  if (ti >= 0) {
    dst->SetField(ti, 1);
  }
  const OGRwkbGeometryType wt = wkbFlatten(src->getGeometryType());
  if (wt == wkbMultiPolygon || wt == wkbPolygon) {
    return dst->SetGeometry(src) == OGRERR_NONE;
  }
  OGRMultiPolygon mp;
  if (auto* coll = dynamic_cast<const OGRGeometryCollection*>(src)) {
    const int n = coll->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      auto* poly = dynamic_cast<OGRPolygon*>(
          const_cast<OGRGeometryCollection*>(coll)->getGeometryRef(i));
      if (poly && mp.addGeometry(poly) != OGRERR_NONE) {
        return false;
      }
    }
    return dst->SetGeometry(&mp) == OGRERR_NONE;
  }
  return dst->SetGeometry(src) == OGRERR_NONE;
}

OGRGeometry* feature_kind_traits<gis::SmtFtTin>::decode_geom(OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  return geom ? geom->clone() : nullptr;
}

bool encode_grid_mesh(const geo::Grid* grid, OGRFeature* dst) {
  if (!grid || grid->is_empty()) {
    return false;
  }
  return dst->SetGeometry(&grid->ogr()) == OGRERR_NONE;
}

bool feature_kind_traits<gis::SmtFtGrid>::encode_geom(const OGRGeometry* src,
                                                      OGRFeature* dst) {
  return src && dst && dst->SetGeometry(src) == OGRERR_NONE;
}

OGRGeometry* feature_kind_traits<gis::SmtFtGrid>::decode_geom(OGRFeature* src) {
  OGRGeometry* geom = src->GetGeometryRef();
  return geom ? geom->clone() : nullptr;
}

bool feature_kind_traits<gis::SmtFtChildImage>::encode_geom(
    const OGRGeometry* /*src*/, OGRFeature* /*dst*/) {
  return false;
}

OGRGeometry* feature_kind_traits<gis::SmtFtChildImage>::decode_geom(
    OGRFeature* /*src*/) {
  return nullptr;
}

bool encode_smt_geometry(const OGRGeometry* src, OGRFeature* dst,
                         gis::SmtFeatureType ft) {
  if (!src || !dst) {
    return false;
  }
  return visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    return Traits::encode_geom(src, dst);
  });
}

bool encode_smt_geometry(const geo::Tin* src, OGRFeature* dst,
                         gis::SmtFeatureType /*ft*/) {
  return encode_tin_mesh(src, dst);
}

bool encode_smt_geometry(const geo::Grid* src, OGRFeature* dst,
                         gis::SmtFeatureType /*ft*/) {
  return encode_grid_mesh(src, dst);
}

OGRGeometry* decode_ogr_geometry(OGRFeature* src, gis::SmtFeatureType hint) {
  if (!src) {
    return nullptr;
  }
  const gis::SmtFeatureType ft = infer_feature_type(src, hint);
  OGRGeometry* out = nullptr;
  visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    out = Traits::decode_geom(src);
    return out != nullptr;
  });
  return out;
}

geo::Tin* decode_smt_tin(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom) {
    return nullptr;
  }
  auto add_triangle_from_ring = [](geo::Tin* tin, OGRLinearRing* ring) {
    if (!ring || ring->getNumPoints() < 3) {
      return;
    }
    OGRPoint p0(ring->getX(0), ring->getY(0));
    OGRPoint p1(ring->getX(1), ring->getY(1));
    OGRPoint p2(ring->getX(2), ring->getY(2));
    tin->add_point(&p0);
    const int ia = tin->get_point_count() - 1;
    tin->add_point(&p1);
    const int ib = tin->get_point_count() - 1;
    tin->add_point(&p2);
    const int ic = tin->get_point_count() - 1;
    base::SmtTriangle tri;
    tri.a = ia;
    tri.b = ib;
    tri.c = ic;
    tin->add_triangle(&tri);
  };

  auto* tin = new geo::Tin();
  const OGRwkbGeometryType wt = wkbFlatten(geom->getGeometryType());
  if (auto* multi = dynamic_cast<OGRMultiPolygon*>(geom)) {
    const int n = multi->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      auto* poly = dynamic_cast<OGRPolygon*>(multi->getGeometryRef(i));
      add_triangle_from_ring(tin, poly ? poly->getExteriorRing() : nullptr);
    }
  } else if (auto* poly = dynamic_cast<OGRPolygon*>(geom)) {
    add_triangle_from_ring(tin, poly->getExteriorRing());
  } else if (wt == wkbTIN) {
    auto* coll = dynamic_cast<OGRGeometryCollection*>(geom);
    const int n = coll ? coll->getNumGeometries() : 0;
    for (int i = 0; i < n && coll; ++i) {
      auto* part = dynamic_cast<OGRPolygon*>(coll->getGeometryRef(i));
      add_triangle_from_ring(tin, part ? part->getExteriorRing() : nullptr);
    }
  } else {
    delete tin;
    return nullptr;
  }
  if (tin->get_triangle_count() < 1 && tin->get_point_count() < 3) {
    delete tin;
    return nullptr;
  }
  return tin;
}

geo::Grid* decode_smt_grid(OGRFeature* src) {
  if (!src) {
    return nullptr;
  }
  OGRGeometry* geom = src->GetGeometryRef();
  if (!geom || wkbFlatten(geom->getGeometryType()) != wkbMultiPoint) {
    return nullptr;
  }
  int rows = 0;
  int cols = 0;
  const int ri = src->GetFieldIndex("grid_row");
  const int ci = src->GetFieldIndex("grid_col");
  if (ri >= 0) {
    rows = src->GetFieldAsInteger(ri);
  }
  if (ci >= 0) {
    cols = src->GetFieldAsInteger(ci);
  }
  auto* mp = dynamic_cast<OGRMultiPoint*>(geom);
  if (!mp) {
    return nullptr;
  }
  const int n = mp->getNumGeometries();
  if (rows <= 0 || cols <= 0) {
    cols = n;
    rows = 1;
  }
  auto* grid = new geo::Grid(rows, cols);
  int k = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols && k < n; ++c, ++k) {
      auto* pt = dynamic_cast<OGRPoint*>(mp->getGeometryRef(k));
      if (!pt) {
        continue;
      }
      geo::RawPoint raw;
      raw.x = pt->getX();
      raw.y = pt->getY();
      grid->set_node(r, c, raw);
    }
  }
  return grid;
}

bool create_vector_layer(GDALDataset* ds, const char* name,
                         gis::SmtFeatureType ft, OGRLayer** out) {
  if (!ds || !name || !out) {
    return false;
  }
  *out = nullptr;
  return visit_feature_kind(ft, [&](auto traits) {
    using Traits = decltype(traits);
    if (Traits::is_raster) {
      return false;
    }
    OGRLayer* lyr = ds->CreateLayer(name, nullptr, Traits::wkb, nullptr);
    if (!lyr) {
      return false;
    }
    OGRFieldDefn style("style", OFTBinary);
    lyr->CreateField(&style);
    for_each_extra_field<typename Traits::extra_fields>([&](auto field) {
      OGRFieldDefn defn(field.name, field.ogr_type);
      lyr->CreateField(&defn);
    });
    *out = lyr;
    return true;
  });
}

OGRLayer* create_scratch_layer(GDALDataset* ds, const char* name) {
  if (!ds || !name) {
    return nullptr;
  }
  OGRLayer* lyr = ds->CreateLayer(name, nullptr, wkbUnknown, nullptr);
  if (!lyr) {
    return nullptr;
  }
  OGRFieldDefn style("style", OFTBinary);
  lyr->CreateField(&style);
  return lyr;
}

}  // namespace datasource
}  // namespace gis
