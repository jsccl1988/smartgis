// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/tile_disk_cache.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <vector>

namespace sdb {
namespace tile {
namespace {

constexpr uint32_t k_magic = 0x54444b31u;  // "TDK1"
constexpr uint32_t k_version = 1;

struct DiskHeader {
  uint32_t magic = k_magic;
  uint32_t version = k_version;
  int32_t z = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t image_code = 4;
  float lb_x = 0;
  float lb_y = 0;
  float rt_x = 0;
  float rt_y = 0;
  uint32_t body_size = 0;
};

}  // namespace

std::filesystem::path TileDiskCache::file_path(const TileCoord& coord) const {
  return std::filesystem::path(directory_) /
         (std::to_string(coord.z) + "_" + std::to_string(coord.x) + "_" +
          std::to_string(coord.y) + ".tile");
}

bool TileDiskCache::try_get(const TileCoord& coord, TileImage* out) const {
  if (!is_enabled() || out == nullptr) {
    return false;
  }
  const auto path = file_path(coord);
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  DiskHeader hdr;
  in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (!in || hdr.magic != k_magic || hdr.version != k_version ||
      hdr.z != coord.z || hdr.x != coord.x || hdr.y != coord.y) {
    return false;
  }
  std::string body(hdr.body_size, '\0');
  if (hdr.body_size > 0) {
    in.read(body.data(), static_cast<std::streamsize>(hdr.body_size));
    if (!in) {
      return false;
    }
  }
  out->coord = coord;
  out->image_code = hdr.image_code;
  out->world_rect.lb.x = hdr.lb_x;
  out->world_rect.lb.y = hdr.lb_y;
  out->world_rect.rt.x = hdr.rt_x;
  out->world_rect.rt.y = hdr.rt_y;
  out->bytes = std::move(body);
  return !out->bytes.empty();
}

void TileDiskCache::put(const TileImage& image) const {
  if (!is_enabled() || image.bytes.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) {
    return;
  }
  DiskHeader hdr;
  hdr.z = image.coord.z;
  hdr.x = image.coord.x;
  hdr.y = image.coord.y;
  hdr.image_code = static_cast<int32_t>(image.image_code);
  hdr.lb_x = image.world_rect.lb.x;
  hdr.lb_y = image.world_rect.lb.y;
  hdr.rt_x = image.world_rect.rt.x;
  hdr.rt_y = image.world_rect.rt.y;
  hdr.body_size = static_cast<uint32_t>(image.bytes.size());

  const auto path = file_path(image.coord);
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return;
    }
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    out.write(image.bytes.data(),
              static_cast<std::streamsize>(image.bytes.size()));
    if (!out) {
      return;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return;
  }
  evict_overflow();
}

void TileDiskCache::clear() const {
  if (!is_enabled()) {
    return;
  }
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory_, ec)) {
    if (ec) {
      break;
    }
    if (entry.is_regular_file(ec) && entry.path().extension() == ".tile") {
      std::filesystem::remove(entry.path(), ec);
    }
  }
}

void TileDiskCache::evict_overflow() const {
  if (!is_enabled()) {
    return;
  }
  std::error_code ec;
  struct Item {
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
  };
  std::vector<Item> items;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory_, ec)) {
    if (ec) {
      return;
    }
    if (!entry.is_regular_file(ec) || entry.path().extension() != ".tile") {
      continue;
    }
    items.push_back(Item{entry.path(), entry.last_write_time(ec)});
  }
  if (items.size() <= capacity_) {
    return;
  }
  std::sort(items.begin(), items.end(),
            [](const Item& a, const Item& b) { return a.mtime < b.mtime; });
  const size_t remove_n = items.size() - capacity_;
  for (size_t i = 0; i < remove_n; ++i) {
    std::filesystem::remove(items[i].path, ec);
  }
}

}  // namespace tile
}  // namespace sdb
