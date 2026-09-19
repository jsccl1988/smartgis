// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/assets/tileset.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace gis {
namespace {

struct Parser {
  const char* cur;
  const char* end;

  void skip() {
    while (cur < end) {
      const char c = *cur;
      if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        break;
      }
      ++cur;
    }
  }

  bool eat(char c) {
    skip();
    if (cur < end && *cur == c) {
      ++cur;
      return true;
    }
    return false;
  }

  bool parse_string(std::string& out) {
    skip();
    if (!eat('"')) {
      return false;
    }
    out.clear();
    while (cur < end && *cur != '"') {
      if (*cur == '\\' && cur + 1 < end) {
        ++cur;
        out.push_back(*cur++);
      } else {
        out.push_back(*cur++);
      }
    }
    return eat('"');
  }

  bool parse_number(double& out) {
    skip();
    if (cur >= end) {
      return false;
    }
    char* stop = nullptr;
    out = std::strtod(cur, &stop);
    if (stop == cur) {
      return false;
    }
    cur = stop;
    return true;
  }

  bool skip_value() {
    skip();
    if (cur >= end) {
      return false;
    }
    if (*cur == '"') {
      std::string tmp;
      return parse_string(tmp);
    }
    if (*cur == '{') {
      return skip_object();
    }
    if (*cur == '[') {
      return skip_array();
    }
    if (std::strncmp(cur, "true", 4) == 0) {
      cur += 4;
      return true;
    }
    if (std::strncmp(cur, "false", 5) == 0) {
      cur += 5;
      return true;
    }
    if (std::strncmp(cur, "null", 4) == 0) {
      cur += 4;
      return true;
    }
    double n;
    return parse_number(n);
  }

  bool skip_object() {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':') || !skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool skip_array() {
    if (!eat('[')) {
      return false;
    }
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      if (!skip_value()) {
        return false;
      }
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_box_aabb(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    double v[12];
    for (int i = 0; i < 12; ++i) {
      if (!parse_number(v[i])) {
        return false;
      }
      skip();
      if (i < 11 && !eat(',')) {
        return false;
      }
    }
    if (!eat(']')) {
      return false;
    }
    const double cx = v[0];
    const double cy = v[1];
    const double cz = v[2];
    const double hx = std::fabs(v[3]) + std::fabs(v[6]) + std::fabs(v[9]);
    const double hy = std::fabs(v[4]) + std::fabs(v[7]) + std::fabs(v[10]);
    const double hz = std::fabs(v[5]) + std::fabs(v[8]) + std::fabs(v[11]);
    tile.min_x = cx - hx;
    tile.max_x = cx + hx;
    tile.min_y = cy - hy;
    tile.max_y = cy + hy;
    tile.min_z = cz - hz;
    tile.max_z = cz + hz;
    return true;
  }

  bool parse_region_aabb(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    double v[6];
    for (int i = 0; i < 6; ++i) {
      if (!parse_number(v[i])) {
        return false;
      }
      skip();
      if (i < 5 && !eat(',')) {
        return false;
      }
    }
    if (!eat(']')) {
      return false;
    }
    tile.min_x = v[0];
    tile.min_y = v[1];
    tile.min_z = v[4];
    tile.max_x = v[2];
    tile.max_y = v[3];
    tile.max_z = v[5];
    return true;
  }

  bool parse_bounding_volume(Tile& tile) {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    bool ok_vol = false;
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "box") {
        ok_vol = parse_box_aabb(tile);
      } else if (key == "region") {
        ok_vol = parse_region_aabb(tile);
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return ok_vol;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_content(Tile& tile) {
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "uri") {
        if (!parse_string(tile.content_uri)) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_children(Tile& tile) {
    if (!eat('[')) {
      return false;
    }
    skip();
    if (eat(']')) {
      return true;
    }
    for (;;) {
      Tile child;
      if (!parse_tile(child)) {
        return false;
      }
      tile.children.push_back(std::move(child));
      skip();
      if (eat(']')) {
        return true;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_tile(Tile& tile) {
    tile = Tile();
    if (!eat('{')) {
      return false;
    }
    skip();
    if (eat('}')) {
      return true;
    }
    bool saw_volume = false;
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "boundingVolume") {
        saw_volume = parse_bounding_volume(tile);
        if (!saw_volume) {
          return false;
        }
      } else if (key == "geometricError") {
        if (!parse_number(tile.geometric_error)) {
          return false;
        }
      } else if (key == "refine") {
        std::string refine;
        if (!parse_string(refine)) {
          return false;
        }
        tile.refine = (refine == "ADD") ? Refine::kAdd : Refine::kReplace;
      } else if (key == "content") {
        if (!parse_content(tile)) {
          return false;
        }
      } else if (key == "children") {
        if (!parse_children(tile)) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return saw_volume;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }

  bool parse_tileset(Tileset& out) {
    skip();
    if (!eat('{')) {
      return false;
    }
    skip();
    bool have_root = false;
    if (eat('}')) {
      return false;
    }
    for (;;) {
      std::string key;
      if (!parse_string(key) || !eat(':')) {
        return false;
      }
      if (key == "root") {
        have_root = parse_tile(out.root);
        if (!have_root) {
          return false;
        }
      } else if (!skip_value()) {
        return false;
      }
      skip();
      if (eat('}')) {
        return have_root;
      }
      if (!eat(',')) {
        return false;
      }
    }
  }
};

double aabb_center_distance(const Tile& tile, const ViewState& view) {
  const double cx = 0.5 * (tile.min_x + tile.max_x);
  const double cy = 0.5 * (tile.min_y + tile.max_y);
  const double cz = 0.5 * (tile.min_z + tile.max_z);
  const double dx = cx - view.eye_x;
  const double dy = cy - view.eye_y;
  const double dz = cz - view.eye_z;
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  return d < 1e-6 ? 1e-6 : d;
}

void select_walk(const Tile& tile, const ViewState& view, double max_sse,
                 std::vector<const Tile*>& visible) {
  const double sse = tile.geometric_error * view.sse_denominator /
                     aabb_center_distance(tile, view);
  const bool refine = sse > max_sse && !tile.children.empty();
  if (!refine) {
    visible.push_back(&tile);
    return;
  }
  if (tile.refine == Refine::kAdd) {
    visible.push_back(&tile);
  }
  for (const Tile& child : tile.children) {
    select_walk(child, view, max_sse, visible);
  }
}

}  // namespace

bool parse_tileset_json(const char* json, size_t len, Tileset& out) {
  out = Tileset();
  if (!json || len == 0) {
    return false;
  }
  Parser p;
  p.cur = json;
  p.end = json + len;
  return p.parse_tileset(out);
}

void select_tiles(const Tileset& tileset, const ViewState& view, double max_sse,
                  std::vector<const Tile*>& visible) {
  visible.clear();
  select_walk(tileset.root, view, max_sse, visible);
}

}  // namespace gis
