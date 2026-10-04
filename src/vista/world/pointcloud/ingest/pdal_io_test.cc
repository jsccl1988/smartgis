// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "vista/world/pointcloud/ingest/pdal_io.h"

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

std::string write_tiny_las(const char* path) {
  std::vector<uint8_t> bytes;
  bytes.reserve(512);
  const char sig[4] = {'L', 'A', 'S', 'F'};
  bytes.insert(bytes.end(), sig, sig + 4);
  bytes.insert(bytes.end(), 20, 0);
  bytes.push_back(1);
  bytes.push_back(2);
  bytes.insert(bytes.end(), 68, 0);
  write_u16(&bytes, 227);
  write_u32(&bytes, 227);
  bytes.insert(bytes.end(), 4, 0);
  bytes.push_back(0);
  write_u16(&bytes, 20);
  write_u32(&bytes, 3);
  while (bytes.size() < 131) {
    bytes.push_back(0);
  }
  write_f64(&bytes, 0.01);
  write_f64(&bytes, 0.01);
  write_f64(&bytes, 0.01);
  write_f64(&bytes, 0.0);
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

}  // namespace

int main() {
  if (!vista::pdal_is_available()) {
    vista::PointCloud cloud;
    expect(!vista::run_pdal_read("missing.las", {}, &cloud), "stub fails");
    expect(cloud.error == "pdal_not_built", "stub error");
    if (g_fails) {
      std::fprintf(stderr, "pdal_io_test: %d failed\n", g_fails);
      return 1;
    }
    std::fprintf(stdout, "pdal_io_test: stub ok (pdal_not_built)\n");
    return 0;
  }

  const std::string las_path =
      (std::filesystem::temp_directory_path() / "pdal_tiny_test.las").string();
  write_tiny_las(las_path.c_str());
  vista::PointCloud cloud;
  expect(vista::run_pdal_read(las_path.c_str(), {}, &cloud), "pdal read las");
  expect(cloud.point_count() == 3, "pdal point count");
  expect(cloud.xyz[0] > 99.f && cloud.xyz[0] < 101.f, "pdal x0");

  std::error_code ec;
  std::filesystem::remove(las_path, ec);

  if (g_fails) {
    std::fprintf(stderr, "pdal_io_test: %d failed (%s)\n", g_fails,
                 cloud.error.empty() ? "-" : cloud.error.c_str());
    return 1;
  }
  std::fprintf(stdout, "pdal_io_test: ok (pdal live)\n");
  return 0;
}
