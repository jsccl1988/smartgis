// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/stratum_tin.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace detail {
namespace {

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool write_text_file(std::string_view path, std::string_view text) {
  if (path.empty()) {
    return false;
  }
  std::ofstream out(std::string(path), std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(out);
}

struct Pt {
  double x = 0;
  double y = 0;
  double z = 0;
};

struct Tri {
  int a = 0;
  int b = 0;
  int c = 0;
};

struct Edge {
  int u = 0;
  int v = 0;
  bool operator==(const Edge& o) const {
    return (u == o.u && v == o.v) || (u == o.v && v == o.u);
  }
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

bool circumcircle_contains(const Pt& a,
                           const Pt& b,
                           const Pt& c,
                           const Pt& p) {
  // Robust-ish: translate so a is origin.
  const double bx = b.x - a.x;
  const double by = b.y - a.y;
  const double cx = c.x - a.x;
  const double cy = c.y - a.y;
  const double px = p.x - a.x;
  const double py = p.y - a.y;
  const double bl = bx * bx + by * by;
  const double cl = cx * cx + cy * cy;
  const double d = 2.0 * (bx * cy - by * cx);
  if (std::fabs(d) < 1e-18) {
    return false;  // collinear — treat as outside
  }
  const double ux = (cy * bl - by * cl) / d;
  const double uy = (bx * cl - cx * bl) / d;
  const double r2 = ux * ux + uy * uy;
  const double dist2 = (px - ux) * (px - ux) + (py - uy) * (py - uy);
  return dist2 <= r2 * (1.0 + 1e-12);
}

double orient2d(const Pt& a, const Pt& b, const Pt& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// Bowyer–Watson Delaunay in XY; Z rides along on vertices.
bool build_delaunay(const std::vector<Pt>& pts, std::vector<Tri>* tris) {
  if (!tris || pts.size() < 3) {
    return false;
  }
  tris->clear();

  double min_x = pts[0].x;
  double min_y = pts[0].y;
  double max_x = pts[0].x;
  double max_y = pts[0].y;
  for (const Pt& p : pts) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }
  const double dx = std::max(max_x - min_x, 1.0);
  const double dy = std::max(max_y - min_y, 1.0);
  const double d = std::max(dx, dy) * 10.0;
  const double mid_x = 0.5 * (min_x + max_x);
  const double mid_y = 0.5 * (min_y + max_y);

  std::vector<Pt> work = pts;
  const int s0 = static_cast<int>(work.size());
  work.push_back({mid_x - 2.0 * d, mid_y - d, 0.0});
  work.push_back({mid_x + 2.0 * d, mid_y - d, 0.0});
  work.push_back({mid_x, mid_y + 2.0 * d, 0.0});
  tris->push_back({s0, s0 + 1, s0 + 2});

  for (int pi = 0; pi < s0; ++pi) {
    std::vector<Edge> polygon;
    std::vector<Tri> next;
    next.reserve(tris->size());
    for (const Tri& t : *tris) {
      if (circumcircle_contains(work[static_cast<size_t>(t.a)],
                                work[static_cast<size_t>(t.b)],
                                work[static_cast<size_t>(t.c)],
                                work[static_cast<size_t>(pi)])) {
        polygon.push_back({t.a, t.b});
        polygon.push_back({t.b, t.c});
        polygon.push_back({t.c, t.a});
      } else {
        next.push_back(t);
      }
    }
    // Keep boundary edges of the polygonal hole (appear once).
    std::vector<Edge> boundary;
    for (size_t i = 0; i < polygon.size(); ++i) {
      bool unique = true;
      for (size_t j = 0; j < polygon.size(); ++j) {
        if (i == j) {
          continue;
        }
        if (polygon[i] == polygon[j]) {
          unique = false;
          break;
        }
      }
      if (unique) {
        boundary.push_back(polygon[i]);
      }
    }
    for (const Edge& e : boundary) {
      // Ensure CCW orientation for later area checks.
      if (orient2d(work[static_cast<size_t>(e.u)],
                   work[static_cast<size_t>(e.v)],
                   work[static_cast<size_t>(pi)]) > 0.0) {
        next.push_back({e.u, e.v, pi});
      } else {
        next.push_back({e.v, e.u, pi});
      }
    }
    *tris = std::move(next);
  }

  // Drop any triangle that touches the super-triangle.
  std::vector<Tri> kept;
  kept.reserve(tris->size());
  for (const Tri& t : *tris) {
    if (t.a >= s0 || t.b >= s0 || t.c >= s0) {
      continue;
    }
    // Reject near-degenerate triangles.
    if (std::fabs(orient2d(work[static_cast<size_t>(t.a)],
                           work[static_cast<size_t>(t.b)],
                           work[static_cast<size_t>(t.c)])) < 1e-18) {
      continue;
    }
    kept.push_back(t);
  }
  *tris = std::move(kept);
  return !tris->empty();
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

  std::vector<Tri> tris;
  if (!build_delaunay(pts, &tris)) {
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
  for (const Tri& t : tris) {
    out.indices.push_back(static_cast<uint32_t>(t.a));
    out.indices.push_back(static_cast<uint32_t>(t.b));
    out.indices.push_back(static_cast<uint32_t>(t.c));
  }
  out.ok = true;
  return out;
}

bool run_stratum_interpolate_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string stratum_id;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "stratum_id", &stratum_id) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  const BoreholeSet holes = load_boreholes_csv(input);
  const StratumTin tin = interpolate_stratum_tin(holes, stratum_id);
  if (!tin.ok) {
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("stratum_id");
  w.String(tin.stratum_id.c_str());
  w.Key("xyz");
  w.StartArray();
  for (double v : tin.xyz) {
    w.Double(v);
  }
  w.EndArray();
  w.Key("indices");
  w.StartArray();
  for (uint32_t i : tin.indices) {
    w.Uint(i);
  }
  w.EndArray();
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

}  // namespace detail
}  // namespace gis
