// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/processing/ops_runner.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string write_square_geojson(const std::filesystem::path& path) {
  std::ofstream out(path);
  out << R"({
  "type": "FeatureCollection",
  "features": [{
    "type": "Feature",
    "properties": {},
    "geometry": {
      "type": "Polygon",
      "coordinates": [[[0,0],[2,0],[2,2],[0,2],[0,0]]]
    }
  }]
})";
  out.close();
  return path.string();
}

std::string write_clip_geojson(const std::filesystem::path& path) {
  std::ofstream out(path);
  out << R"({
  "type": "FeatureCollection",
  "features": [{
    "type": "Feature",
    "properties": {},
    "geometry": {
      "type": "Polygon",
      "coordinates": [[[1,1],[3,1],[3,3],[1,3],[1,1]]]
    }
  }]
})";
  out.close();
  return path.string();
}

std::string json_path(const std::string& p) {
  std::string s = p;
  for (char& c : s) {
    if (c == '\\') {
      c = '/';
    }
  }
  return s;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = dir / "smartgis_m2_in.geojson";
  const fs::path clip = dir / "smartgis_m2_clip.geojson";
  const fs::path out_buf = dir / "smartgis_m2_buffer.geojson";
  const fs::path out_clip = dir / "smartgis_m2_clip_out.geojson";
  const fs::path out_cent = dir / "smartgis_m2_cent.geojson";
  const fs::path out_env = dir / "smartgis_m2_env.geojson";

  write_square_geojson(in);
  write_clip_geojson(clip);

  expect(plugin::builtin_op_catalog().size() >= 10, "catalog >= 10");

  const std::string buf_args =
      std::string("{\"input\":\"") + json_path(in.string()) +
      "\",\"output\":\"" + json_path(out_buf.string()) +
      "\",\"distance\":0.5}";
  expect(plugin::run_builtin_op("native.buffer", buf_args), "buffer op");
  expect(fs::exists(out_buf), "buffer wrote file");

  const std::string clip_args =
      std::string("{\"input\":\"") + json_path(in.string()) +
      "\",\"output\":\"" + json_path(out_clip.string()) + "\",\"clip\":\"" +
      json_path(clip.string()) + "\"}";
  expect(plugin::run_builtin_op("native.clip", clip_args), "clip op");
  expect(fs::exists(out_clip), "clip wrote file");

  expect(plugin::run_builtin_op(
             "native.centroid",
             std::string("{\"input\":\"") + json_path(in.string()) +
                 "\",\"output\":\"" + json_path(out_cent.string()) + "\"}"),
         "centroid op");
  expect(fs::exists(out_cent), "centroid wrote file");

  expect(plugin::run_builtin_op(
             "native.envelope",
             std::string("{\"input\":\"") + json_path(in.string()) +
                 "\",\"output\":\"" + json_path(out_env.string()) + "\"}"),
         "envelope op");
  expect(fs::exists(out_env), "envelope wrote file");

  std::error_code ec;
  fs::remove(in, ec);
  fs::remove(clip, ec);
  fs::remove(out_buf, ec);
  fs::remove(out_clip, ec);
  fs::remove(out_cent, ec);
  fs::remove(out_env, ec);

  if (g_fails) {
    std::fprintf(stderr, "processing_ops_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("processing_ops_test: ok");
  return 0;
}
