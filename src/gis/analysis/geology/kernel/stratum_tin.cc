// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gis/analysis/geology/kernel/stratum_tin.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "gis/geo/tin/delaunay.h"

namespace gis {
namespace {

struct Pt {
  double x = 0;
  double y = 0;
  double z = 0;
};

struct XyKey {
  long long x = 0;
  long long y = 0;
  bool operator==(const XyKey& o) const { return x == o.x && y == o.y; }
};

struct XyKeyHash {
  size_t operator()(const XyKey& k) const {
    return (static_cast<size_t>(k.x) * 1315423911u) ^
           static_cast<size_t>(k.y);
  }
};

constexpr double kSnap = 1e-9;

XyKey make_xy_key(double x, double y) {
  return {static_cast<long long>(std::llround(x / kSnap)),
          static_cast<long long>(std::llround(y / kSnap))};
}

}  // namespace

StratumTin interpolate_stratum_tin(const BoreholeSet& holes,
                                   std::string_view stratum_id) {
  StratumTin out;
  out.stratum_id = std::string(stratum_id);
  if (!holes.ok) {
    out.error = holes.error.empty() ? "invalid borehole set" : holes.error;
    return out;
  }
  if (stratum_id.empty()) {
    out.error = "empty stratum_id";
    return out;
  }

  std::unordered_map<XyKey, size_t, XyKeyHash> dedup;
  std::vector<Pt> pts;
  for (const BoreholeContact& c : holes.contacts) {
    if (c.stratum_id != stratum_id) {
      continue;
    }
    const XyKey key = make_xy_key(c.x, c.y);
    const auto it = dedup.find(key);
    if (it != dedup.end()) {
      // Average Z for coincident XY samples of the same stratum.
      Pt& p = pts[it->second];
      p.z = 0.5 * (p.z + c.z);
      continue;
    }
    dedup.emplace(key, pts.size());
    pts.push_back({c.x, c.y, c.z});
  }

  if (pts.size() < 3) {
    out.error = "need at least 3 contacts for stratum TIN";
    return out;
  }

  std::vector<base::Vector3> verts(pts.size());
  for (size_t i = 0; i < pts.size(); ++i) {
    verts[i].x = static_cast<float>(pts[i].x);
    verts[i].y = static_cast<float>(pts[i].y);
    verts[i].z = static_cast<float>(pts[i].z);
  }
  std::vector<geo::IndexedTriangle> tris;
  if (!geo::delaunay_triangles(tris, verts.data(),
                               static_cast<int>(verts.size())) ||
      tris.empty()) {
    out.error = "Delaunay triangulation failed (collinear?)";
    return out;
  }

  out.xyz.reserve(pts.size() * 3);
  for (const Pt& p : pts) {
    out.xyz.push_back(p.x);
    out.xyz.push_back(p.y);
    out.xyz.push_back(p.z);
  }
  out.indices.reserve(tris.size() * 3);
  for (const geo::IndexedTriangle& t : tris) {
    out.indices.push_back(static_cast<uint32_t>(t.a));
    out.indices.push_back(static_cast<uint32_t>(t.b));
    out.indices.push_back(static_cast<uint32_t>(t.c));
  }
  out.ok = true;
  return out;
}

}  // namespace gis
