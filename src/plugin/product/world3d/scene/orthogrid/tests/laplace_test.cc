// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/orthogrid/solve/boundary_solve.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_boundary_file_solves_rectangle() {
  const char* path = "orthogrid_boundary_fixture.txt";
  {
    std::ofstream out(path);
    out << "gridbnd:\n";
    out << "5 5\n";
    out << "begin\n";
    out << "main_begin\n";
    out << "2\n0 4 0 0 1 1\n0,0\n4,0\n";
    out << "2\n0 4 4 1 1 1\n4,0\n4,4\n";
    out << "2\n4 0 4 2 1 1\n4,4\n0,4\n";
    out << "2\n4 0 0 3 1 1\n0,4\n0,0\n";
    out << "main_end\nend\n";
  }
  const plugin::detail::BoundarySolve solved =
      plugin::detail::solve_grid_boundary_file(path);
  expect(solved.ok, "boundary file solves");
  expect(solved.node_count == 25, "boundary node count");
  expect(solved.message.find("\"nodes\":25") != std::string::npos,
         "boundary reports nodes");
  expect(!solved.cell_orth.empty(), "cell orth populated");
  expect(!solved.raster_orth.empty(), "raster orth populated");
  expect(solved.xs.size() == 25 && solved.ys.size() == 25,
         "boundary keeps vector coords");

  {
    std::ofstream out(path);
    out << "gridbnd:\n";
  }
  const plugin::detail::BoundarySolve header_only =
      plugin::detail::solve_grid_boundary_file(path);
  expect(!header_only.ok, "header-only file is not a laplace smoke");
  std::remove(path);
}

}  // namespace

int main() {
  test_boundary_file_solves_rectangle();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}
