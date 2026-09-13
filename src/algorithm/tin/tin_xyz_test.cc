// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/tin/xyz_points.h"

#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string temp_xyz_path() {
#ifdef _WIN32
  char dir[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, dir) == 0) {
    return "tin_xyz_test.tmp";
  }
  return std::string(dir) + "tin_xyz_test.txt";
#else
  return "tin_xyz_test.tmp";
#endif
}

}  // namespace

int main() {
  const std::string path = temp_xyz_path();
  {
    std::ofstream out(path);
    expect(static_cast<bool>(out), "open temp xyz");
    out << "1.0 2.0 3.0\n";
    out << "4.0 5.0 6.0\n";
    out << "7.0 8.0 9.0\n";
  }

  std::vector<render::Vector3> pts;
  expect(tin::read_xyz_points(path.c_str(), 0, ' ', 0, 1, 2, &pts) ==
             SMT_ERR_NONE,
         "read_xyz_points three lines");
  expect(pts.size() == 3, "three Vector3s");
  if (pts.size() == 3) {
    expect(pts[0].x == 1.f && pts[0].y == 2.f && pts[0].z == 3.f, "row 0");
    expect(pts[1].x == 4.f && pts[1].y == 5.f && pts[1].z == 6.f, "row 1");
    expect(pts[2].x == 7.f && pts[2].y == 8.f && pts[2].z == 9.f, "row 2");
  }

  expect(tin::read_xyz_points("no_such_xyz_file.xyz", 0, ',', 0, 1, 2, &pts) ==
             SMT_ERR_INVALID_FILE,
         "missing file");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
