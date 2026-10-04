// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/pointcloud/ingest/laz_io.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "lasunzipper.hpp"
#include "laszip.hpp"

namespace vista {
namespace {

uint16_t read_u16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t read_u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

int32_t read_i32(const uint8_t* p) {
  return static_cast<int32_t>(read_u32(p));
}

double read_f64(const uint8_t* p) {
  double v = 0;
  std::memcpy(&v, p, 8);
  return v;
}

bool load_laszip_vlr(const uint8_t* file, size_t file_size,
                     uint16_t header_size, uint32_t num_vlrs, LASzip* laszip,
                     std::string* err) {
  if (!file || !laszip || !err) {
    return false;
  }
  size_t off = header_size;
  for (uint32_t i = 0; i < num_vlrs; ++i) {
    if (off + 54 > file_size) {
      *err = "vlr_truncated";
      return false;
    }
    const char* user_id = reinterpret_cast<const char*>(file + off + 2);
    const uint16_t record_id = read_u16(file + off + 18);
    const uint16_t record_len = read_u16(file + off + 20);
    if (off + 54 + record_len > file_size) {
      *err = "vlr_data_truncated";
      return false;
    }
    if (std::strncmp(user_id, "laszip encoded", 14) == 0 &&
        record_id == 22204) {
      if (!laszip->unpack(file + off + 54, record_len)) {
        *err = laszip->get_error() ? laszip->get_error() : "laszip_unpack";
        return false;
      }
      return true;
    }
    off += 54 + record_len;
  }
  *err = "laszip_vlr_missing";
  return false;
}

}  // namespace

bool load_las_via_laszip(const char* path, const LasLoadOptions& options,
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

  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    out->error = "open_failed";
    return false;
  }
  const std::streamsize file_size_s = in.tellg();
  if (file_size_s < 227) {
    out->error = "short_file";
    return false;
  }
  const size_t file_size = static_cast<size_t>(file_size_s);
  in.seekg(0);
  std::vector<uint8_t> bytes(file_size);
  in.read(reinterpret_cast<char*>(bytes.data()),
          static_cast<std::streamsize>(file_size));
  if (!in) {
    out->error = "read_failed";
    return false;
  }
  if (std::memcmp(bytes.data(), "LASF", 4) != 0) {
    out->error = "bad_magic";
    return false;
  }

  const uint16_t header_size = read_u16(bytes.data() + 94);
  const uint32_t offset_to_points = read_u32(bytes.data() + 96);
  const uint32_t num_vlrs = read_u32(bytes.data() + 100);
  const uint8_t point_format = bytes[104];
  const uint16_t point_length = read_u16(bytes.data() + 105);
  uint64_t point_count = read_u32(bytes.data() + 107);
  if (bytes[25] >= 4 && point_count == 0 && header_size >= 375) {
    std::memcpy(&point_count, bytes.data() + 247, 8);
  }
  if (point_count == 0 || point_length < 20 ||
      offset_to_points >= file_size) {
    out->error = "bad_point_layout";
    return false;
  }

  const double scale_x = read_f64(bytes.data() + 131);
  const double scale_y = read_f64(bytes.data() + 139);
  const double scale_z = read_f64(bytes.data() + 147);
  const double offset_x = read_f64(bytes.data() + 155);
  const double offset_y = read_f64(bytes.data() + 163);
  const double offset_z = read_f64(bytes.data() + 171);

  LASzip laszip;
  if (!load_laszip_vlr(bytes.data(), file_size, header_size, num_vlrs, &laszip,
                       &out->error)) {
    // Fallback: try default setup (uncompressed LAZ is rare).
    if (!laszip.setup(point_format, point_length,
                      LASZIP_COMPRESSOR_DEFAULT)) {
      if (out->error.empty()) {
        out->error = laszip.get_error() ? laszip.get_error() : "laszip_setup";
      }
      return false;
    }
    out->error.clear();
  }

  FILE* fp = nullptr;
#if defined(_MSC_VER)
  if (fopen_s(&fp, path, "rb") != 0 || !fp) {
    out->error = "fopen_failed";
    return false;
  }
#else
  fp = std::fopen(path, "rb");
  if (!fp) {
    out->error = "fopen_failed";
    return false;
  }
#endif
  if (std::fseek(fp, static_cast<long>(offset_to_points), SEEK_SET) != 0) {
    out->error = "seek_failed";
    std::fclose(fp);
    return false;
  }

  LASunzipper unzipper;
  if (!unzipper.open(fp, &laszip)) {
    out->error = unzipper.get_error() ? unzipper.get_error() : "unzip_open";
    std::fclose(fp);
    return false;
  }

  if (laszip.num_items < 1 || !laszip.items) {
    out->error = "laszip_no_items";
    unzipper.close();
    std::fclose(fp);
    return false;
  }

  std::vector<std::vector<uint8_t>> item_bufs(laszip.num_items);
  std::vector<uint8_t*> ptrs(laszip.num_items);
  for (unsigned short i = 0; i < laszip.num_items; ++i) {
    item_bufs[i].assign(laszip.items[i].size, 0);
    ptrs[i] = item_bufs[i].data();
  }

  // RGB often lives in a dedicated item after POINT10/POINT14.
  int rgb_item = -1;
  for (unsigned short i = 0; i < laszip.num_items; ++i) {
    if (laszip.items[i].type == LASitem::RGB12 ||
        laszip.items[i].type == LASitem::RGB14 ||
        laszip.items[i].type == LASitem::RGBNIR14) {
      rgb_item = static_cast<int>(i);
      break;
    }
  }

  const size_t stride = options.stride == 0 ? 1 : options.stride;
  const size_t max_keep =
      options.max_points == 0 ? point_count : options.max_points;
  // Avoid Windows min/max macros clobbering std::min.
  const size_t reserve_pts =
      point_count < max_keep ? static_cast<size_t>(point_count) : max_keep;
  out->xyz.reserve(reserve_pts * 3);
  if (rgb_item >= 0) {
    out->rgba.reserve(reserve_pts * 4);
  }

  size_t kept = 0;
  for (uint64_t i = 0; i < point_count; ++i) {
    if (!unzipper.read(ptrs.data())) {
      break;
    }
    if ((i % stride) != 0) {
      continue;
    }
    // First item always carries XYZ integers for standard LASZIP layouts.
    if (item_bufs[0].size() < 12) {
      continue;
    }
    const int32_t xi = read_i32(item_bufs[0].data());
    const int32_t yi = read_i32(item_bufs[0].data() + 4);
    const int32_t zi = read_i32(item_bufs[0].data() + 8);
    out->xyz.push_back(
        static_cast<float>(static_cast<double>(xi) * scale_x + offset_x));
    out->xyz.push_back(
        static_cast<float>(static_cast<double>(yi) * scale_y + offset_y));
    out->xyz.push_back(
        static_cast<float>(static_cast<double>(zi) * scale_z + offset_z));
    if (rgb_item >= 0 && item_bufs[static_cast<size_t>(rgb_item)].size() >= 6) {
      const uint8_t* rgb = item_bufs[static_cast<size_t>(rgb_item)].data();
      const uint16_t r = read_u16(rgb);
      const uint16_t g = read_u16(rgb + 2);
      const uint16_t b = read_u16(rgb + 4);
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

  unzipper.close();
  std::fclose(fp);

  if (out->empty()) {
    out->error = "no_points";
    return false;
  }
  out->recompute_bounds();
  return true;
}

}  // namespace vista
