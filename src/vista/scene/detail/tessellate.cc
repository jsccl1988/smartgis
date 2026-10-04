// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/scene/detail/tessellate.h"

#include "vista/scene/detail/paint.h"

#include "ogrsf_frmts.h"

#include <cmath>
#include <vector>

namespace vista {
namespace detail {

// Expand xyz-only positions to interleaved POSITION+NORMAL (6 floats/vert)
// for the lit program. Accumulates area-weighted face normals, then
// normalizes; zero-length falls back to +Y so the lit VS never sees 0.
void interleave_positions_with_normals(const float* positions,
                                       size_t position_count,
                                       const uint32_t* indices,
                                       size_t index_count,
                                       std::vector<float>* out) {
  if (!out || !positions || position_count < 3 || (position_count % 3) != 0) {
    return;
  }
  const size_t verts = position_count / 3;
  std::vector<float> normals(verts * 3, 0.f);
  if (indices && index_count >= 3) {
    for (size_t t = 0; t + 2 < index_count; t += 3) {
      const uint32_t i0 = indices[t];
      const uint32_t i1 = indices[t + 1];
      const uint32_t i2 = indices[t + 2];
      if (i0 >= verts || i1 >= verts || i2 >= verts) {
        continue;
      }
      const float* p0 = positions + i0 * 3;
      const float* p1 = positions + i1 * 3;
      const float* p2 = positions + i2 * 3;
      const float e1x = p1[0] - p0[0];
      const float e1y = p1[1] - p0[1];
      const float e1z = p1[2] - p0[2];
      const float e2x = p2[0] - p0[0];
      const float e2y = p2[1] - p0[1];
      const float e2z = p2[2] - p0[2];
      const float nx = e1y * e2z - e1z * e2y;
      const float ny = e1z * e2x - e1x * e2z;
      const float nz = e1x * e2y - e1y * e2x;
      normals[i0 * 3 + 0] += nx;
      normals[i0 * 3 + 1] += ny;
      normals[i0 * 3 + 2] += nz;
      normals[i1 * 3 + 0] += nx;
      normals[i1 * 3 + 1] += ny;
      normals[i1 * 3 + 2] += nz;
      normals[i2 * 3 + 0] += nx;
      normals[i2 * 3 + 1] += ny;
      normals[i2 * 3 + 2] += nz;
    }
  }
  out->resize(verts * 6);
  for (size_t i = 0; i < verts; ++i) {
    float nx = normals[i * 3 + 0];
    float ny = normals[i * 3 + 1];
    float nz = normals[i * 3 + 2];
    const float len2 = nx * nx + ny * ny + nz * nz;
    if (len2 > 1e-12f) {
      const float inv = 1.f / std::sqrt(len2);
      nx *= inv;
      ny *= inv;
      nz *= inv;
    } else {
      nx = 0.f;
      ny = 1.f;
      nz = 0.f;
    }
    (*out)[i * 6 + 0] = positions[i * 3 + 0];
    (*out)[i * 6 + 1] = positions[i * 3 + 1];
    (*out)[i * 6 + 2] = positions[i * 3 + 2];
    (*out)[i * 6 + 3] = nx;
    (*out)[i * 6 + 4] = ny;
    (*out)[i * 6 + 5] = nz;
  }
}

// Gap: tessellate.h has no radius-aware point API (tessellate_point_xy is
// fixed world half-extent 0.05). Approximate circle-radius (pixels) as a
// diamond (4 tris) scaled by world_units_per_pixel.
void append_circle_diamond(double x, double y, double z, double radius_world,
                           vista::TessMesh& out) {
  if (radius_world <= 0) {
    return;
  }
  const float r = static_cast<float>(radius_world);
  const uint32_t base =
      static_cast<uint32_t>(out.positions.size() / 3);
  const float fx = static_cast<float>(x);
  const float fy = static_cast<float>(y);
  const float fz = static_cast<float>(z);
  // Center + N/E/S/W.
  out.positions.push_back(fx);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.positions.push_back(fx);
  out.positions.push_back(fy + r);
  out.positions.push_back(fz);
  out.positions.push_back(fx + r);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.positions.push_back(fx);
  out.positions.push_back(fy - r);
  out.positions.push_back(fz);
  out.positions.push_back(fx - r);
  out.positions.push_back(fy);
  out.positions.push_back(fz);
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
  out.indices.push_back(base);
  out.indices.push_back(base + 2);
  out.indices.push_back(base + 3);
  out.indices.push_back(base);
  out.indices.push_back(base + 3);
  out.indices.push_back(base + 4);
  out.indices.push_back(base);
  out.indices.push_back(base + 4);
  out.indices.push_back(base + 1);
}

void append_mesh(vista::TessMesh& dst, const vista::TessMesh& src) {
  if (src.indices.empty()) {
    return;
  }
  const uint32_t base = static_cast<uint32_t>(dst.positions.size() / 3);
  dst.positions.insert(dst.positions.end(), src.positions.begin(),
                       src.positions.end());
  for (uint32_t idx : src.indices) {
    dst.indices.push_back(base + idx);
  }
}

bool tessellate_geom_paint_aware(const OGRGeometry* geom,
                                 const gis::style::ResolvedPaint* paint,
                                 double world_units_per_pixel,
                                 vista::TessMesh& out) {
  if (!geom) {
    return false;
  }
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (paint && paint->type == gis::style::LayerType::kLine &&
      (flat == wkbLineString || flat == wkbLinearRing)) {
    // tessellate_line resets its out mesh — always stage then merge.
    vista::TessMesh part;
    const vista::LineTessOptions options =
        line_options_from_paint(*paint, world_units_per_pixel);
    if (!vista::tessellate_line(geom->toLineString(), options, part)) {
      return false;
    }
    append_mesh(out, part);
    return true;
  }
  if (paint && paint->type == gis::style::LayerType::kCircle &&
      flat == wkbPoint) {
    const auto* pt = geom->toPoint();
    const double radius_world =
        static_cast<double>(paint->circle_radius) * world_units_per_pixel;
    const size_t before = out.indices.size();
    append_circle_diamond(pt->getX(), pt->getY(), pt->getZ(), radius_world,
                          out);
    return out.indices.size() > before;
  }
  if (flat == wkbMultiPoint || flat == wkbMultiLineString ||
      flat == wkbMultiPolygon || flat == wkbGeometryCollection ||
      flat == wkbTIN) {
    const auto* col = geom->toGeometryCollection();
    if (!col) {
      return false;
    }
    bool any = false;
    const int n = col->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      if (tessellate_geom_paint_aware(col->getGeometryRef(i), paint,
                                      world_units_per_pixel, out)) {
        any = true;
      }
    }
    return any;
  }
  // Fill / unknown / non-styled geom types keep legacy tessellate_geometry.
  vista::TessMesh part;
  if (!vista::tessellate_geometry(geom, part)) {
    return false;
  }
  append_mesh(out, part);
  return true;
}

bool tessellate_vector_instance(const GpuInstance& inst,
                                double world_units_per_pixel,
                                vista::TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  out.has_image = false;
  const gis::style::ResolvedPaint* paint =
      inst.has_paint ? &inst.paint : nullptr;
  const bool style_stroke =
      paint && (paint->type == gis::style::LayerType::kLine ||
                paint->type == gis::style::LayerType::kCircle);

  if (!style_stroke) {
    // Fill path: ear-clip with view resolution so concave admin rings
    // (Inner Mongolia) keep a coherent frontier instead of fan+PIP chords.
    vista::FillTessOptions fill_opts;
    fill_opts.world_units_per_pixel = world_units_per_pixel;
    fill_opts.max_fan_verts = 1024;
    if (inst.ogr_layer) {
      inst.ogr_layer->ResetReading();
      bool any = false;
      while (OGRFeature* feat = inst.ogr_layer->GetNextFeature()) {
        vista::TessMesh part;
        if (vista::tessellate_geometry(feat->GetGeometryRef(), fill_opts,
                                     part)) {
          append_mesh(out, part);
          any = true;
        }
        OGRFeature::DestroyFeature(feat);
      }
      return any && !out.indices.empty();
    }
    if (!inst.geoms.empty()) {
      bool any = false;
      for (const OGRGeometry* g : inst.geoms) {
        vista::TessMesh part;
        if (vista::tessellate_geometry(g, fill_opts, part)) {
          append_mesh(out, part);
          any = true;
        }
      }
      return any && !out.indices.empty();
    }
    return false;
  }

  if (inst.ogr_layer) {
    inst.ogr_layer->ResetReading();
    bool any = false;
    while (OGRFeature* feat = inst.ogr_layer->GetNextFeature()) {
      if (tessellate_geom_paint_aware(feat->GetGeometryRef(), paint,
                                      world_units_per_pixel, out)) {
        any = true;
      }
      OGRFeature::DestroyFeature(feat);
    }
    return any && !out.indices.empty();
  }
  if (!inst.geoms.empty()) {
    bool any = false;
    for (const OGRGeometry* g : inst.geoms) {
      if (tessellate_geom_paint_aware(g, paint, world_units_per_pixel, out)) {
        any = true;
      }
    }
    return any && !out.indices.empty();
  }
  return false;
}

}  // namespace detail
}  // namespace vista
