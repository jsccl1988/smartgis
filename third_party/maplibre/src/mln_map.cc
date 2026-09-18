// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "mln/map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// Do not include <mln/map/map.hpp> here: that is the pin's mln::Map and
// would collide with this facade. Native construction is documented in PIN.txt.
// Still-image scope matches adapter background path only (no tile fetch).

namespace mln {
namespace {

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool parse_hash_rgb(const char* p, uint8_t* b, uint8_t* g, uint8_t* r,
                    uint8_t* a) {
  if (!p || p[0] != '#') {
    return false;
  }
  ++p;
  int n = 0;
  while (p[n] && n < 8) {
    if (hex_nibble(p[n]) < 0) {
      break;
    }
    ++n;
  }
  auto hx = [&](int i) { return hex_nibble(p[i]); };
  if (n == 6) {
    *r = static_cast<uint8_t>((hx(0) << 4) | hx(1));
    *g = static_cast<uint8_t>((hx(2) << 4) | hx(3));
    *b = static_cast<uint8_t>((hx(4) << 4) | hx(5));
    *a = 0xFF;
    return true;
  }
  if (n == 3) {
    *r = static_cast<uint8_t>(hx(0) * 17);
    *g = static_cast<uint8_t>(hx(1) * 17);
    *b = static_cast<uint8_t>(hx(2) * 17);
    *a = 0xFF;
    return true;
  }
  return false;
}

float clamp01(float v) {
  return std::max(0.f, std::min(1.f, v));
}

// Lightweight parse: facade must not depend on //src/sdb/style.
void resolve_background(const std::string& json, uint8_t* b, uint8_t* g,
                        uint8_t* r, uint8_t* a) {
  *b = 0x40;
  *g = 0x80;
  *r = 0xC0;
  *a = 0xFF;
  const char* key = std::strstr(json.c_str(), "background-color");
  if (key) {
    const char* hash = std::strchr(key, '#');
    if (hash) {
      parse_hash_rgb(hash, b, g, r, a);
    }
  }
  const char* op_key = std::strstr(json.c_str(), "background-opacity");
  if (!op_key) {
    return;
  }
  const char* colon = std::strchr(op_key, ':');
  if (!colon) {
    return;
  }
  char* stop = nullptr;
  const float opacity = clamp01(static_cast<float>(std::strtod(colon + 1, &stop)));
  if (stop == colon + 1) {
    return;
  }
  *a = static_cast<uint8_t>(std::lround(static_cast<float>(*a) * opacity));
}

}  // namespace

Map::Map(uint32_t width_px, uint32_t height_px)
    : width_px_(width_px), height_px_(height_px) {
}

Map::~Map() = default;

void Map::load_style_json(const char* json) {
  style_json_ = json ? json : "";
}

bool Map::render_still(uint8_t* bgra, uint32_t stride_bytes) const {
  if (!bgra || width_px_ == 0 || height_px_ == 0) {
    return false;
  }
  if (stride_bytes < width_px_ * 4) {
    return false;
  }
  uint8_t b = 0;
  uint8_t g = 0;
  uint8_t r = 0;
  uint8_t a = 0;
  resolve_background(style_json_, &b, &g, &r, &a);
  for (uint32_t y = 0; y < height_px_; ++y) {
    uint8_t* row = bgra + static_cast<size_t>(y) * stride_bytes;
    for (uint32_t x = 0; x < width_px_; ++x) {
      row[x * 4 + 0] = b;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = r;
      row[x * 4 + 3] = a;
    }
  }
  return true;
}

}  // namespace mln
