// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/pointcloud/ingest/io/las_io.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "gis/vista/world/pointcloud/ingest/io/laz_io.h"

namespace gis {
namespace {

uint16_t read_u16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t read_u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

int32_t read_i32(const uint8_t* p) {
  return static_cast<int32_t>(read_u32(p));
}

double read_f64(const uint8_t* p) {
  double v = 0;
  static_assert(sizeof(double) == 8, "IEEE754 double");
  std::memcpy(&v, p, 8);
  return v;
}

bool ends_with_ci(const std::string& s, const char* suffix) {
  const size_t n = std::strlen(suffix);
  if (s.size() < n) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    char a = s[s.size() - n + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool load_las_file(const char* path, const LasLoadOptions& options,
                   PointCloud* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!path || !*path) {
    out->error = "empty_path";
    return false;
  }
  out->source_path = path;
  if (ends_with_ci(out->source_path, ".laz")) {
    return load_las_via_laszip(path, options, out);
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    out->error = "open_failed";
    return false;
  }
  std::vector<uint8_t> header(375, 0);
  in.read(reinterpret_cast<char*>(header.data()), 227);
  if (!in || in.gcount() < 227) {
    out->error = "short_header";
    return false;
  }
  if (std::memcmp(header.data(), "LASF", 4) != 0) {
    out->error = "bad_magic";
    return false;
  }

  const uint8_t ver_major = header[24];
  const uint8_t ver_minor = header[25];
  if (ver_major != 1 || ver_minor > 4) {
    out->error = "unsupported_version";
    return false;
  }

  const uint16_t header_size = read_u16(header.data() + 94);
  const uint32_t offset_to_points = read_u32(header.data() + 96);
  const uint8_t point_format = header[104];
  const uint16_t point_length = read_u16(header.data() + 105);
  uint64_t point_count = read_u32(header.data() + 107);

  if (header_size > 227) {
    const size_t extra = static_cast<size_t>(header_size) - 227;
    if (header.size() < header_size) {
      header.resize(header_size);
    }
    in.read(reinterpret_cast<char*>(header.data() + 227),
            static_cast<std::streamsize>(extra));
    if (!in || static_cast<size_t>(in.gcount()) < extra) {
      out->error = "short_header_ext";
      return false;
    }
  }

  // LAS 1.4 extended point count (when legacy field is zero).
  if (ver_minor >= 4 && point_count == 0 && header_size >= 375) {
    uint64_t n = 0;
    std::memcpy(&n, header.data() + 247, 8);
    point_count = n;
  }

  if (point_format > 3 && point_format != 6 && point_format != 7) {
    // P0: common formats only (0-3); 6/7 (LAS 1.4) handled below loosely.
    if (point_format > 7) {
      out->error = "unsupported_point_format";
      return false;
    }
  }
  if (point_length < 20 || point_count == 0 || offset_to_points < header_size) {
    out->error = "bad_point_layout";
    return false;
  }

  const double scale_x = read_f64(header.data() + 131);
  const double scale_y = read_f64(header.data() + 139);
  const double scale_z = read_f64(header.data() + 147);
  const double offset_x = read_f64(header.data() + 155);
  const double offset_y = read_f64(header.data() + 163);
  const double offset_z = read_f64(header.data() + 171);

  const bool has_rgb =
      (point_format == 2 || point_format == 3 || point_format == 7);
  size_t rgb_offset = 0;
  if (point_format == 2) {
    rgb_offset = 20;
  } else if (point_format == 3) {
    rgb_offset = 28;
  } else if (point_format == 7) {
    rgb_offset = 30;
  }
  if (has_rgb && point_length < rgb_offset + 6) {
    out->error = "rgb_truncated";
    return false;
  }

  in.seekg(static_cast<std::streamoff>(offset_to_points), std::ios::beg);
  if (!in) {
    out->error = "seek_points_failed";
    return false;
  }

  const size_t stride = options.stride == 0 ? 1 : options.stride;
  const size_t max_keep = options.max_points == 0 ? point_count : options.max_points;
  out->xyz.reserve(std::min(point_count, max_keep) * 3);
  if (has_rgb) {
    out->rgba.reserve(std::min(point_count, max_keep) * 4);
  }

  std::vector<uint8_t> record(point_length);
  size_t kept = 0;
  for (uint64_t i = 0; i < point_count; ++i) {
    in.read(reinterpret_cast<char*>(record.data()), point_length);
    if (!in || static_cast<size_t>(in.gcount()) < point_length) {
      break;
    }
    if ((i % stride) != 0) {
      continue;
    }
    const int32_t xi = read_i32(record.data());
    const int32_t yi = read_i32(record.data() + 4);
    const int32_t zi = read_i32(record.data() + 8);
    const float x =
        static_cast<float>(static_cast<double>(xi) * scale_x + offset_x);
    const float y =
        static_cast<float>(static_cast<double>(yi) * scale_y + offset_y);
    const float z =
        static_cast<float>(static_cast<double>(zi) * scale_z + offset_z);
    out->xyz.push_back(x);
    out->xyz.push_back(y);
    out->xyz.push_back(z);
    if (has_rgb) {
      const uint16_t r = read_u16(record.data() + rgb_offset);
      const uint16_t g = read_u16(record.data() + rgb_offset + 2);
      const uint16_t b = read_u16(record.data() + rgb_offset + 4);
      out->rgba.push_back(static_cast<uint8_t>(r > 255 ? r >> 8 : r));
      out->rgba.push_back(static_cast<uint8_t>(g > 255 ? g >> 8 : g));
      out->rgba.push_back(static_cast<uint8_t>(b > 255 ? b >> 8 : b));
      out->rgba.push_back(255);
    }
    ++kept;
    if (kept >= max_keep) {
      break;
    }
  }

  if (out->empty()) {
    out->error = "no_points";
    return false;
  }
  out->recompute_bounds();
  return true;
}

}  // namespace gis
