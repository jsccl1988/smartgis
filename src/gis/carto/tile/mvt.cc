// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/carto/tile/mvt.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include "zlib.h"

namespace gis {
namespace tile {
namespace {

enum WireType : uint32_t {
  kVarint = 0,
  kFixed64 = 1,
  kLengthDelimited = 2,
  kFixed32 = 5,
};

struct Cursor {
  const uint8_t* p = nullptr;
  const uint8_t* end = nullptr;

  bool empty() const { return p >= end; }
  size_t remain() const {
    return p < end ? static_cast<size_t>(end - p) : 0;
  }
};

bool read_varint(Cursor* c, uint64_t* out) {
  if (!c || !out) {
    return false;
  }
  uint64_t value = 0;
  int shift = 0;
  while (!c->empty() && shift < 64) {
    const uint8_t byte = *c->p++;
    value |= static_cast<uint64_t>(byte & 0x7fu) << shift;
    if ((byte & 0x80u) == 0) {
      *out = value;
      return true;
    }
    shift += 7;
  }
  return false;
}

bool read_bytes(Cursor* c, size_t n, Cursor* out_slice) {
  if (!c || !out_slice || c->remain() < n) {
    return false;
  }
  out_slice->p = c->p;
  out_slice->end = c->p + n;
  c->p += n;
  return true;
}

bool skip_field(Cursor* c, uint32_t wire) {
  uint64_t v = 0;
  switch (wire) {
    case kVarint:
      return read_varint(c, &v);
    case kFixed64:
      if (c->remain() < 8) {
        return false;
      }
      c->p += 8;
      return true;
    case kFixed32:
      if (c->remain() < 4) {
        return false;
      }
      c->p += 4;
      return true;
    case kLengthDelimited: {
      if (!read_varint(c, &v) || c->remain() < v) {
        return false;
      }
      c->p += static_cast<size_t>(v);
      return true;
    }
    default:
      return false;
  }
}

int32_t zigzag_decode(uint32_t n) {
  return static_cast<int32_t>((n >> 1) ^ (~(n & 1) + 1));
}

bool maybe_gunzip(const uint8_t* data, size_t len, std::vector<uint8_t>* out,
                  bool* was_gzip) {
  if (!data || !out || !was_gzip) {
    return false;
  }
  *was_gzip = false;
  out->clear();
  if (len >= 2 && data[0] == 0x1f && data[1] == 0x8b) {
    *was_gzip = true;
    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    strm.avail_in = static_cast<uInt>(len);
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
      return false;
    }
    out->resize(len * 4 + 64);
    int rc = Z_OK;
    while (rc != Z_STREAM_END) {
      if (strm.total_out >= out->size()) {
        out->resize(out->size() * 2 + 64);
      }
      strm.next_out = out->data() + strm.total_out;
      strm.avail_out = static_cast<uInt>(out->size() - strm.total_out);
      rc = inflate(&strm, Z_NO_FLUSH);
      if (rc != Z_OK && rc != Z_STREAM_END) {
        inflateEnd(&strm);
        out->clear();
        return false;
      }
    }
    out->resize(static_cast<size_t>(strm.total_out));
    inflateEnd(&strm);
    return true;
  }
  out->assign(data, data + len);
  return true;
}

bool parse_value(Cursor* c, std::string* out) {
  if (!c || !out) {
    return false;
  }
  out->clear();
  while (!c->empty()) {
    uint64_t tag = 0;
    if (!read_varint(c, &tag)) {
      return false;
    }
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7u);
    if (field == 1 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      out->assign(reinterpret_cast<const char*>(slice.p),
                  static_cast<size_t>(slice.end - slice.p));
    } else if (field == 7 && wire == kVarint) {
      uint64_t v = 0;
      if (!read_varint(c, &v)) {
        return false;
      }
      *out = v ? "true" : "false";
    } else if ((field == 4 || field == 5 || field == 6) && wire == kVarint) {
      uint64_t v = 0;
      if (!read_varint(c, &v)) {
        return false;
      }
      if (field == 6) {
        *out = std::to_string(zigzag_decode(static_cast<uint32_t>(v)));
      } else {
        *out = std::to_string(v);
      }
    } else if (field == 2 && wire == kFixed32) {
      if (c->remain() < 4) {
        return false;
      }
      float f = 0.f;
      std::memcpy(&f, c->p, 4);
      c->p += 4;
      *out = std::to_string(f);
    } else if (field == 3 && wire == kFixed64) {
      if (c->remain() < 8) {
        return false;
      }
      double d = 0.0;
      std::memcpy(&d, c->p, 8);
      c->p += 8;
      *out = std::to_string(d);
    } else if (!skip_field(c, wire)) {
      return false;
    }
  }
  return true;
}

bool decode_geometry(const std::vector<uint32_t>& cmds, MvtGeomType type,
                     std::vector<std::vector<std::pair<double, double>>>* rings) {
  if (!rings) {
    return false;
  }
  rings->clear();
  double x = 0;
  double y = 0;
  std::vector<std::pair<double, double>> cur;
  size_t i = 0;
  while (i < cmds.size()) {
    const uint32_t command = cmds[i++];
    const uint32_t id = command & 0x7u;
    const uint32_t count = command >> 3;
    if (id == 1 || id == 2) {  // MoveTo / LineTo
      for (uint32_t n = 0; n < count; ++n) {
        if (i + 1 >= cmds.size()) {
          return false;
        }
        x += zigzag_decode(cmds[i++]);
        y += zigzag_decode(cmds[i++]);
        if (id == 1 && !cur.empty()) {
          rings->push_back(std::move(cur));
          cur.clear();
        }
        cur.emplace_back(x, y);
      }
    } else if (id == 7) {  // ClosePath
      if (!cur.empty()) {
        if (cur.front() != cur.back()) {
          cur.push_back(cur.front());
        }
        rings->push_back(std::move(cur));
        cur.clear();
      }
    } else {
      return false;
    }
  }
  if (!cur.empty()) {
    rings->push_back(std::move(cur));
  }
  if (rings->empty()) {
    return type == MvtGeomType::kUnknown;
  }
  return true;
}

bool parse_feature(Cursor* c, const std::vector<std::string>& keys,
                   const std::vector<std::string>& values, MvtFeature* out) {
  if (!c || !out) {
    return false;
  }
  std::vector<uint32_t> tags;
  std::vector<uint32_t> geom;
  uint64_t geom_type = 0;
  while (!c->empty()) {
    uint64_t tag = 0;
    if (!read_varint(c, &tag)) {
      return false;
    }
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7u);
    if (field == 1 && wire == kVarint) {
      uint64_t id = 0;
      if (!read_varint(c, &id)) {
        return false;
      }
      out->id = id;
    } else if (field == 2 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      while (!slice.empty()) {
        uint64_t v = 0;
        if (!read_varint(&slice, &v)) {
          return false;
        }
        tags.push_back(static_cast<uint32_t>(v));
      }
    } else if (field == 3 && wire == kVarint) {
      if (!read_varint(c, &geom_type)) {
        return false;
      }
    } else if (field == 4 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      while (!slice.empty()) {
        uint64_t v = 0;
        if (!read_varint(&slice, &v)) {
          return false;
        }
        geom.push_back(static_cast<uint32_t>(v));
      }
    } else if (!skip_field(c, wire)) {
      return false;
    }
  }
  out->type = static_cast<MvtGeomType>(geom_type);
  for (size_t i = 0; i + 1 < tags.size(); i += 2) {
    const uint32_t ki = tags[i];
    const uint32_t vi = tags[i + 1];
    if (ki < keys.size() && vi < values.size()) {
      out->attrs[keys[ki]] = values[vi];
    }
  }
  return decode_geometry(geom, out->type, &out->rings);
}

bool parse_layer(Cursor* c, MvtLayer* out) {
  if (!c || !out) {
    return false;
  }
  std::vector<std::string> keys;
  std::vector<std::string> values;
  std::vector<Cursor> feature_slices;
  while (!c->empty()) {
    uint64_t tag = 0;
    if (!read_varint(c, &tag)) {
      return false;
    }
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7u);
    if (field == 1 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      out->name.assign(reinterpret_cast<const char*>(slice.p),
                       static_cast<size_t>(slice.end - slice.p));
    } else if (field == 2 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      feature_slices.push_back(slice);
    } else if (field == 3 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      keys.emplace_back(reinterpret_cast<const char*>(slice.p),
                        static_cast<size_t>(slice.end - slice.p));
    } else if (field == 4 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      std::string value;
      if (!parse_value(&slice, &value)) {
        return false;
      }
      values.push_back(std::move(value));
    } else if (field == 5 && wire == kVarint) {
      uint64_t v = 0;
      if (!read_varint(c, &v)) {
        return false;
      }
      out->extent = static_cast<uint32_t>(v);
    } else if (field == 15 && wire == kVarint) {
      uint64_t v = 0;
      if (!read_varint(c, &v)) {
        return false;
      }
      out->version = static_cast<uint32_t>(v);
    } else if (!skip_field(c, wire)) {
      return false;
    }
  }
  out->features.reserve(feature_slices.size());
  for (Cursor slice : feature_slices) {
    MvtFeature feature;
    if (!parse_feature(&slice, keys, values, &feature)) {
      return false;
    }
    out->features.push_back(std::move(feature));
  }
  return true;
}

bool parse_tile(Cursor* c, MvtTile* out) {
  if (!c || !out) {
    return false;
  }
  out->layers.clear();
  while (!c->empty()) {
    uint64_t tag = 0;
    if (!read_varint(c, &tag)) {
      return false;
    }
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7u);
    if (field == 3 && wire == kLengthDelimited) {
      uint64_t n = 0;
      Cursor slice;
      if (!read_varint(c, &n) || !read_bytes(c, static_cast<size_t>(n), &slice)) {
        return false;
      }
      MvtLayer layer;
      if (!parse_layer(&slice, &layer)) {
        return false;
      }
      out->layers.push_back(std::move(layer));
    } else if (!skip_field(c, wire)) {
      return false;
    }
  }
  return true;
}

const char* type_name(MvtGeomType t) {
  switch (t) {
    case MvtGeomType::kPoint:
      return "point";
    case MvtGeomType::kLineString:
      return "linestring";
    case MvtGeomType::kPolygon:
      return "polygon";
    default:
      return "unknown";
  }
}

}  // namespace

MvtDecodeStatus decode_mvt(const uint8_t* data, size_t len, MvtTile* out) {
  if (!out) {
    return MvtDecodeStatus::kBadInput;
  }
  out->layers.clear();
  if (!data || len == 0) {
    return MvtDecodeStatus::kBadInput;
  }
  std::vector<uint8_t> buf;
  bool was_gzip = false;
  if (!maybe_gunzip(data, len, &buf, &was_gzip)) {
    return MvtDecodeStatus::kUnsupportedEncoding;
  }
  if (buf.empty()) {
    return MvtDecodeStatus::kBadInput;
  }
  Cursor c{buf.data(), buf.data() + buf.size()};
  if (!parse_tile(&c, out)) {
    return was_gzip ? MvtDecodeStatus::kUnsupportedEncoding
                    : MvtDecodeStatus::kBadInput;
  }
  if (out->layers.empty()) {
    return MvtDecodeStatus::kEmpty;
  }
  size_t features = 0;
  for (const MvtLayer& layer : out->layers) {
    features += layer.features.size();
  }
  return features == 0 ? MvtDecodeStatus::kEmpty : MvtDecodeStatus::kOk;
}

bool decode_tile(const uint8_t* data, size_t len,
                 std::vector<std::string>* out_features) {
  MvtTile tile;
  if (decode_mvt(data, len, &tile) != MvtDecodeStatus::kOk) {
    return false;
  }
  if (out_features) {
    out_features->clear();
    for (const MvtLayer& layer : tile.layers) {
      for (const MvtFeature& feature : layer.features) {
        out_features->push_back(layer.name + ":" + type_name(feature.type));
      }
    }
  }
  return true;
}

MvtDecodeStatus decode_status_of(const uint8_t* data, size_t len) {
  MvtTile tile;
  return decode_mvt(data, len, &tile);
}

DecodeStatus decode_status() {
  // Kept for older call sites that probed the stub without bytes.
  return MvtDecodeStatus::kBadInput;
}

StyleSourceStatus reject_vector_source() {
  return StyleSourceStatus::kVectorUnsupported;
}

const char* non_goal_message() {
  return "MVT local decode is available; Style vector URL bind / network "
         "vector TileProvider remains unsupported — use decode_mvt / "
         "decode_mvt_to_map_frame with local tiles.";
}

}  // namespace tile
}  // namespace gis
