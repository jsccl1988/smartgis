// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gis/analysis/geology/stratum_tin.h"

#include <cmath>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "gis/geo/tin/delaunay.h"

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
