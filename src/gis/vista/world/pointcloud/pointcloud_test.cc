// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gis/vista/world/pointcloud/process/chunk.h"
#include "gis/vista/world/pointcloud/ingest/load.h"
#include "gis/vista/world/pointcloud/process/lod.h"
#include "gis/vista/world/terrain/mesh/tessellate.h"
#include "gis/vista/world/world.h"

#include "laszip.hpp"
#include "laszipper.hpp"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void write_u16(std::vector<uint8_t>* buf, uint16_t v) {
  buf->push_back(static_cast<uint8_t>(v & 0xff));
  buf->push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

void write_u32(std::vector<uint8_t>* buf, uint32_t v) {
  buf->push_back(static_cast<uint8_t>(v & 0xff));
  buf->push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  buf->push_back(static_cast<uint8_t>((v >> 16) & 0xff));
  buf->push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void write_i32(std::vector<uint8_t>* buf, int32_t v) {
  write_u32(buf, static_cast<uint32_t>(v));
}

void write_f64(std::vector<uint8_t>* buf, double v) {
  uint8_t raw[8];
  std::memcpy(raw, &v, 8);
  buf->insert(buf->end(), raw, raw + 8);
}

// Minimal LAS 1.2 point format 0 with three points.
std::string write_tiny_las(const char* path) {
  std::vector<uint8_t> bytes;
  bytes.reserve(512);
  const char sig[4] = {'L', 'A', 'S', 'F'};
  bytes.insert(bytes.end(), sig, sig + 4);
  bytes.insert(bytes.end(), 20, 0);  // bytes 4..23
  bytes.push_back(1);                // major @24
  bytes.push_back(2);                // minor @25
  bytes.insert(bytes.end(), 68, 0);  // through byte 93
  write_u16(&bytes, 227);            // header size @94
  write_u32(&bytes, 227);            // offset to points @96
  bytes.insert(bytes.end(), 4, 0);   // number of VLRs @100
  bytes.push_back(0);                // point format @104
  write_u16(&bytes, 20);             // point length @105
  write_u32(&bytes, 3);              // point count @107
  while (bytes.size() < 131) {
    bytes.push_back(0);
  }
  write_f64(&bytes, 0.01);  // scale x/y/z
  write_f64(&bytes, 0.01);
  write_f64(&bytes, 0.01);
  write_f64(&bytes, 0.0);  // offset x/y/z
  write_f64(&bytes, 0.0);
  write_f64(&bytes, 0.0);
  while (bytes.size() < 227) {
    bytes.push_back(0);
  }
  for (int i = 0; i < 3; ++i) {
    write_i32(&bytes, 10000 + i * 100);
    write_i32(&bytes, 20000 + i * 100);
    write_i32(&bytes, 1000 + i * 100);
    write_u16(&bytes, 0);
    bytes.push_back(0);
    bytes.push_back(1);
    bytes.push_back(0);
    bytes.push_back(0);
    write_u16(&bytes, 0);
  }

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return path;
}

// Compress the same three points to LAZ via LASzipper (round-trip for laz_io).
bool write_tiny_laz(const char* path) {
  LASzip zip;
  if (!zip.setup(0, 20, LASZIP_COMPRESSOR_DEFAULT)) {
    std::fprintf(stderr, "laz setup: %s\n",
                 zip.get_error() ? zip.get_error() : "?");
    return false;
  }
  unsigned char* vlr_bytes = nullptr;
  int vlr_size = 0;
  if (!zip.pack(vlr_bytes, vlr_size) || !vlr_bytes || vlr_size < 1) {
    std::fprintf(stderr, "laz pack: %s\n",
                 zip.get_error() ? zip.get_error() : "?");
    return false;
  }
  const uint32_t offset =
      227u + 54u + static_cast<uint32_t>(vlr_size);

  std::vector<uint8_t> head;
  head.reserve(offset);
  const char sig[4] = {'L', 'A', 'S', 'F'};
  head.insert(head.end(), sig, sig + 4);
  head.insert(head.end(), 20, 0);
  head.push_back(1);
  head.push_back(2);
  head.insert(head.end(), 68, 0);
  write_u16(&head, 227);
  write_u32(&head, offset);
  write_u32(&head, 1);  // one VLR
  head.push_back(0);
  write_u16(&head, 20);
  write_u32(&head, 3);
  while (head.size() < 131) {
    head.push_back(0);
  }
  write_f64(&head, 0.01);
  write_f64(&head, 0.01);
  write_f64(&head, 0.01);
  write_f64(&head, 0.0);
  write_f64(&head, 0.0);
  write_f64(&head, 0.0);
  while (head.size() < 227) {
    head.push_back(0);
  }
  // VLR header
  write_u16(&head, 0);
  const char uid[16] = {'l', 'a', 's', 'z', 'i', 'p', ' ', 'e',
                        'n', 'c', 'o', 'd', 'e', 'd', '\0', '\0'};
  head.insert(head.end(), uid, uid + 16);
  write_u16(&head, 22204);
  write_u16(&head, static_cast<uint16_t>(vlr_size));
  head.insert(head.end(), 32, 0);
  // LASzip::pack owns `vlr_bytes` (stored as LASzip::bytes); do not delete[].
  head.insert(head.end(), vlr_bytes, vlr_bytes + vlr_size);

  FILE* fp = nullptr;
#if defined(_MSC_VER)
  if (fopen_s(&fp, path, "wb") != 0 || !fp) {
    return false;
  }
#else
  fp = std::fopen(path, "wb");
  if (!fp) {
    return false;
  }
#endif
  if (std::fwrite(head.data(), 1, head.size(), fp) != head.size()) {
    std::fclose(fp);
    return false;
  }

  LASzipper zipper;
  if (!zipper.open(fp, &zip)) {
    std::fprintf(stderr, "laz zipper open: %s\n",
                 zipper.get_error() ? zipper.get_error() : "?");
    std::fclose(fp);
    return false;
  }
  if (zip.num_items < 1) {
    zipper.close();
    std::fclose(fp);
    return false;
  }
  std::vector<std::vector<uint8_t>> item_bufs(zip.num_items);
  std::vector<uint8_t*> ptrs(zip.num_items);
  for (unsigned short i = 0; i < zip.num_items; ++i) {
    item_bufs[i].assign(zip.items[i].size, 0);
    ptrs[i] = item_bufs[i].data();
  }
  for (int i = 0; i < 3; ++i) {
    // POINT10: X Y Z i16 intensity + flags (20 bytes in item 0).
    uint8_t* p = item_bufs[0].data();
    const int32_t xi = 10000 + i * 100;
    const int32_t yi = 20000 + i * 100;
    const int32_t zi = 1000 + i * 100;
    std::memcpy(p, &xi, 4);
    std::memcpy(p + 4, &yi, 4);
    std::memcpy(p + 8, &zi, 4);
    p[12] = 0;
    p[13] = 0;
    p[14] = 0;
    p[15] = 1;
    p[16] = 0;
    p[17] = 0;
    p[18] = 0;
    p[19] = 0;
    if (!zipper.write(ptrs.data())) {
      std::fprintf(stderr, "laz write: %s\n",
                   zipper.get_error() ? zipper.get_error() : "?");
      zipper.close();
      std::fclose(fp);
      return false;
    }
  }
  if (!zipper.close()) {
    std::fclose(fp);
    return false;
  }
  std::fclose(fp);
  return true;
}

}  // namespace

int main() {
  const auto tmp_dir = std::filesystem::temp_directory_path();
  const std::string las_path =
      (tmp_dir / "pointcloud_tiny_test.las").string();
  const std::string laz_path =
      (tmp_dir / "pointcloud_tiny_test.laz").string();
  const std::string txt_path =
      (tmp_dir / "pointcloud_tiny_test.txt").string();
  write_tiny_las(las_path.c_str());

  gis::PointCloud cloud;
  expect(gis::load_point_cloud(las_path.c_str(), &cloud), "load las");
  expect(cloud.point_count() == 3, "las point count");
  expect(cloud.xyz[0] > 99.f && cloud.xyz[0] < 101.f, "las x0");

  // LAZ round-trip.
  {
    expect(write_tiny_laz(laz_path.c_str()), "write laz");
    gis::PointCloud laz;
    if (!gis::load_point_cloud(laz_path.c_str(), &laz)) {
      std::fprintf(stderr, "FAIL: load laz (%s)\n",
                   laz.error.empty() ? "?" : laz.error.c_str());
      ++g_fails;
    }
    expect(laz.point_count() == 3, "laz point count");
    expect(std::fabs(laz.xyz[0] - cloud.xyz[0]) < 0.02f, "laz x0 match");
  }

  gis::World world;
  gis::Node* node = world.attach_pointcloud(
      "cloud", cloud.min_x, cloud.min_y, cloud.min_z, cloud.max_x, cloud.max_y,
      cloud.max_z);
  expect(node != nullptr, "attach");
  expect(world.set_pointcloud_points(node->id, cloud.xyz.data(),
                                     cloud.point_count(), nullptr, 0),
         "set points");
  expect(node->point_positions.size() == 9, "node xyz");

  gis::TessMesh mesh;
  expect(gis::tessellate_point_cloud(cloud.xyz.data(), cloud.point_count(), 0.1f,
                                     mesh),
         "tessellate");
  expect(mesh.indices.size() == 9, "3 tris");

  // P1 chunks.
  {
    std::vector<gis::PointCloudChunk> chunks;
    gis::PointCloudChunkOptions opts;
    opts.grid_axis = 2;
    opts.max_points_per_chunk = 100;
    expect(gis::build_point_cloud_chunks(cloud, opts, &chunks), "chunks");
    expect(!chunks.empty(), "chunk non-empty");
  }

  // P2 LOD uniform thin.
  {
    gis::PointCloudLodOptions lod;
    lod.max_points = 2;
    lod.focus_radius = 0;
    std::vector<uint32_t> idx;
    expect(gis::select_point_cloud_lod(cloud, lod, &idx), "lod");
    expect(idx.size() == 2, "lod size");
  }

  // set_pointcloud_points builds chunks.
  expect(!node->point_chunks.empty(), "node chunks");

  // Text dialect (leftover comma RGB).
  {
    std::ofstream t(txt_path);
    t << "1.0,2.0,3.0,10,20,30\n";
    t << "4.0,5.0,6.0,40,50,60\n";
    t.close();
    gis::PointCloud text;
    expect(gis::load_point_cloud(txt_path.c_str(), &text), "load txt");
    expect(text.point_count() == 2, "txt count");
    expect(text.has_color(), "txt color");
  }

  // Missing LAZ still fails with a structured error.
  {
    gis::PointCloud laz;
    expect(!gis::load_point_cloud("missing.laz", &laz), "laz missing fails");
    expect(!laz.error.empty(), "laz error set");
  }

  std::error_code ec;
  std::filesystem::remove(las_path, ec);
  std::filesystem::remove(laz_path, ec);
  std::filesystem::remove(txt_path, ec);

  if (g_fails) {
    std::fprintf(stderr, "pointcloud_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "pointcloud_test: ok\n");
  return 0;
}
