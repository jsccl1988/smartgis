// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/prism_volume.h"

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

struct Xy {
  double x = 0;
  double y = 0;
};

struct Xyz {
  double x = 0;
  double y = 0;
  double z = 0;
};

bool xy_less(const Xy& a, const Xy& b) {
  if (a.x != b.x) {
    return a.x < b.x;
  }
  return a.y < b.y;
}

double cross(const Xy& o, const Xy& a, const Xy& b) {
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Andrew's monotone chain; returns closed ring without repeating first point.
std::vector<Xy> convex_hull(std::vector<Xy> pts) {
  std::sort(pts.begin(), pts.end(), xy_less);
  pts.erase(std::unique(pts.begin(), pts.end(),
                        [](const Xy& a, const Xy& b) {
                          return a.x == b.x && a.y == b.y;
                        }),
            pts.end());
  if (pts.size() <= 1) {
    return pts;
  }
  std::vector<Xy> lower;
  for (const Xy& p : pts) {
    while (lower.size() >= 2 &&
           cross(lower[lower.size() - 2], lower.back(), p) <= 0.0) {
      lower.pop_back();
    }
    lower.push_back(p);
  }
  std::vector<Xy> upper;
  for (auto it = pts.rbegin(); it != pts.rend(); ++it) {
    while (upper.size() >= 2 &&
           cross(upper[upper.size() - 2], upper.back(), *it) <= 0.0) {
      upper.pop_back();
    }
    upper.push_back(*it);
  }
  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}

double polygon_area(const std::vector<Xy>& ring) {
  if (ring.size() < 3) {
    return 0.0;
  }
  double sum = 0.0;
  for (size_t i = 0; i < ring.size(); ++i) {
    const Xy& a = ring[i];
    const Xy& b = ring[(i + 1) % ring.size()];
    sum += a.x * b.y - b.x * a.y;
  }
  return 0.5 * std::fabs(sum);
}

double bbox_area(const std::vector<Xy>& pts) {
  if (pts.empty()) {
    return 0.0;
  }
  double min_x = pts[0].x;
  double min_y = pts[0].y;
  double max_x = pts[0].x;
  double max_y = pts[0].y;
  for (const Xy& p : pts) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }
  return std::max(0.0, max_x - min_x) * std::max(0.0, max_y - min_y);
}

}  // namespace

PrismVolumeResult prism_volume_between(const BoreholeSet& holes,
                                       std::string_view top_stratum_id,
                                       std::string_view bottom_stratum_id) {
  PrismVolumeResult out;
  out.stratum_id = std::string(top_stratum_id);
  if (!out.stratum_id.empty() && !bottom_stratum_id.empty()) {
    out.stratum_id += "/" + std::string(bottom_stratum_id);
  }

  if (!holes.ok) {
    out.error = holes.error.empty() ? "invalid borehole set" : holes.error;
    return out;
  }
  if (top_stratum_id.empty() || bottom_stratum_id.empty()) {
    out.error = "empty top/bottom stratum_id";
    return out;
  }
  if (top_stratum_id == bottom_stratum_id) {
    out.error = "top and bottom stratum_id must differ";
    return out;
  }

  std::unordered_map<std::string, Xyz> top;
  std::unordered_map<std::string, Xyz> bottom;
  for (const BoreholeContact& c : holes.contacts) {
    if (c.stratum_id == top_stratum_id) {
      top[c.hole_id] = {c.x, c.y, c.z};
    } else if (c.stratum_id == bottom_stratum_id) {
      bottom[c.hole_id] = {c.x, c.y, c.z};
    }
  }

  std::vector<Xy> xy;
  double sum_thickness = 0.0;
  int n_pair = 0;
  for (const auto& [hole_id, t] : top) {
    const auto it = bottom.find(hole_id);
    if (it == bottom.end()) {
      continue;
    }
    const Xyz& b = it->second;
    const double thickness = t.z - b.z;
    if (!(thickness > 0.0)) {
      continue;
    }
    // Use mid-XY of the paired contacts (usually identical).
    xy.push_back({0.5 * (t.x + b.x), 0.5 * (t.y + b.y)});
    sum_thickness += thickness;
    ++n_pair;
  }

  if (n_pair == 0) {
    out.error = "no hole pairs with positive top-bottom thickness";
    return out;
  }

  const double avg_thickness = sum_thickness / static_cast<double>(n_pair);
  const std::vector<Xy> hull = convex_hull(xy);
  double area = polygon_area(hull);
  if (!(area > 0.0)) {
    area = bbox_area(xy);
  }
  if (!(area > 0.0)) {
    out.error = "degenerate XY footprint";
    return out;
  }

  out.volume = avg_thickness * area;
  out.ok = true;
  return out;
}

bool run_stratum_prism_volume_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string top_id;
  std::string bottom_id;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "top_stratum_id", &top_id) ||
      !json_get_string(args, "bottom_stratum_id", &bottom_id) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  const BoreholeSet holes = load_boreholes_csv(input);
  const PrismVolumeResult vol =
      prism_volume_between(holes, top_id, bottom_id);
  if (!vol.ok) {
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("stratum_id");
  w.String(vol.stratum_id.c_str());
  w.Key("volume");
  w.Double(vol.volume);
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

}  // namespace detail
}  // namespace gis
