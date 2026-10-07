// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Datasets × schemes performance matrix for vista/mesh SIMD + execution.
// Schemes: scalar | avx2 | dispatch (kernels); serial | parallel (batch tess).
// Prints markdown tables to stdout (us / iter, lower is better).

#include "vista/mesh/detail/mesh_append.h"
#include "vista/mesh/detail/mesh_simd.h"
#include "vista/mesh/detail/mesh_simd_kern.h"
#include "vista/mesh/detail/tess_trace.h"
#include "vista/mesh/tessellate.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kWarmup = 3;
constexpr int kIters = 24;

struct Cell {
  const char* dataset = nullptr;
  const char* scheme = nullptr;
  double us = 0;
  bool skip = false;
};

std::vector<Cell> g_cells;

void record(const char* dataset, const char* scheme, double us, bool skip) {
  g_cells.push_back({dataset, scheme, us, skip});
}

template <typename Fn>
double time_us(Fn&& fn, int iters = kIters) {
  for (int i = 0; i < kWarmup; ++i) {
    fn();
  }
  const auto t0 = Clock::now();
  for (int i = 0; i < iters; ++i) {
    fn();
  }
  const auto t1 = Clock::now();
  const double ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count();
  return ns / static_cast<double>(iters) / 1000.0;
}

void make_ring_soa(int n, double scale, std::vector<double>* x,
                   std::vector<double>* y) {
  x->resize(static_cast<size_t>(n));
  y->resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(n);
    (*x)[static_cast<size_t>(i)] =
        scale * std::cos(t * 6.283185307179586);
    (*y)[static_cast<size_t>(i)] =
        scale * std::sin(t * 6.283185307179586) +
        0.08 * scale * std::sin(t * 48.0);
  }
}

std::vector<vista::detail::Vec2> make_dirs(size_t n) {
  std::vector<vista::detail::Vec2> dirs(n);
  for (size_t i = 0; i < n; ++i) {
    const double a = static_cast<double>(i) * 0.017;
    dirs[i] = {std::cos(a) * (1.0 + 0.01 * static_cast<double>(i % 7)),
               std::sin(a) * (1.0 + 0.01 * static_cast<double>(i % 5))};
  }
  return dirs;
}

std::vector<float> make_cloud(size_t n) {
  std::vector<float> xyz(n * 3u);
  for (size_t i = 0; i < n; ++i) {
    xyz[i * 3u + 0u] = static_cast<float>(std::sin(i * 0.13) * 100.0);
    xyz[i * 3u + 1u] = static_cast<float>(std::cos(i * 0.17) * 80.0);
    xyz[i * 3u + 2u] = static_cast<float>((i % 97u) * 0.25);
  }
  return xyz;
}

OGRLinearRing* make_ogr_ring(int n, double scale) {
  auto* ring = new OGRLinearRing();
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(n);
    const double x = scale * std::cos(t * 6.283185307179586);
    const double y = scale * std::sin(t * 6.283185307179586) +
                     0.08 * scale * std::sin(t * 48.0);
    ring->addPoint(x, y);
  }
  ring->closeRings();
  return ring;
}

OGRPolygon* make_ogr_poly(int n, double scale) {
  auto* poly = new OGRPolygon();
  poly->addRingDirectly(make_ogr_ring(n, scale));
  return poly;
}

std::vector<float> make_polyline_xyz(size_t n) {
  std::vector<float> xyz(n * 2u);
  for (size_t i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n);
    xyz[i * 2u + 0u] = t * 1000.f;
    xyz[i * 2u + 1u] =
        std::sin(t * 40.f) * 40.f + std::sin(t * 7.f) * 120.f;
  }
  return xyz;
}

void bench_extrema(const char* dataset, const std::vector<double>& x,
                   const std::vector<double>& y, bool avx2) {
  const int n = static_cast<int>(x.size());
  int i_n = 0, i_s = 0, i_e = 0, i_w = 0;
  record(dataset, "scalar",
         time_us([&] {
           vista::detail::find_xy_extrema_scalar(x.data(), y.data(), n, &i_n,
                                                &i_s, &i_e, &i_w);
         }),
         false);
  record(dataset, "avx2",
         avx2 ? time_us([&] {
           vista::detail::find_xy_extrema_avx2(x.data(), y.data(), n, &i_n,
                                              &i_s, &i_e, &i_w);
         })
              : 0.0,
         !avx2);
  record(dataset, "dispatch",
         time_us([&] {
           vista::detail::find_xy_extrema(x.data(), y.data(), n, &i_n, &i_s,
                                         &i_e, &i_w);
         }),
         false);
}

void bench_rdp(const char* dataset, const std::vector<double>& x,
               const std::vector<double>& y, bool avx2) {
  const int n = static_cast<int>(x.size());
  const double tol2 = 0.25;
  volatile int sink = 0;
  record(dataset, "scalar",
         time_us([&] {
           sink = vista::detail::rdp_farthest_index_scalar(x.data(), y.data(), 0,
                                                          n - 1, tol2);
         }),
         false);
  record(dataset, "avx2",
         avx2 ? time_us([&] {
           sink = vista::detail::rdp_farthest_index_avx2(x.data(), y.data(), 0,
                                                        n - 1, tol2);
         })
              : 0.0,
         !avx2);
  record(dataset, "dispatch",
         time_us([&] {
           sink = vista::detail::rdp_farthest_index(x.data(), y.data(), 0,
                                                   n - 1, tol2);
         }),
         false);
  (void)sink;
}

void bench_normalize(const char* dataset, size_t n, bool avx2) {
  const auto src = make_dirs(n);
  auto run = [&](auto fn) {
    auto dirs = src;
    fn(dirs.data(), dirs.size());
  };
  record(dataset, "scalar",
         time_us([&] { run(vista::detail::normalize_dirs_batch_scalar); }),
         false);
  record(dataset, "avx2",
         avx2 ? time_us([&] { run(vista::detail::normalize_dirs_batch_avx2); })
              : 0.0,
         !avx2);
  record(dataset, "dispatch",
         time_us([&] { run(vista::detail::normalize_dirs_batch); }), false);
}

void bench_path(const char* dataset, const std::vector<double>& x,
                const std::vector<double>& y, bool avx2) {
  const int n = static_cast<int>(x.size());
  volatile double sink = 0;
  record(dataset, "scalar",
         time_us([&] {
           sink = vista::detail::path_length_xy_scalar(x.data(), y.data(), n);
         }),
         false);
  record(dataset, "avx2",
         avx2 ? time_us([&] {
           sink = vista::detail::path_length_xy_avx2(x.data(), y.data(), n);
         })
              : 0.0,
         !avx2);
  record(dataset, "dispatch",
         time_us([&] {
           sink = vista::detail::path_length_xy(x.data(), y.data(), n);
         }),
         false);
  (void)sink;
}

void bench_aabb(const char* dataset, const std::vector<float>& xyz, bool avx2) {
  const size_t n = xyz.size() / 3u;
  float mn[3], mx[3];
  record(dataset, "scalar",
         time_us([&] {
           vista::detail::aabb_xyz_f32_scalar(xyz.data(), n, mn, mx);
         }),
         false);
  record(dataset, "avx2",
         avx2 ? time_us([&] {
           vista::detail::aabb_xyz_f32_avx2(xyz.data(), n, mn, mx);
         })
              : 0.0,
         !avx2);
  record(dataset, "dispatch",
         time_us([&] { vista::detail::aabb_xyz_f32(xyz.data(), n, mn, mx); }),
         false);
}

void bench_line_tess(const char* dataset, size_t n) {
  const auto xyz = make_polyline_xyz(n);
  vista::LineTessOptions opts;
  opts.half_width = 1.0;
  opts.cap = vista::LineCap::kButt;
  opts.join = vista::LineJoin::kBevel;
  opts.world_units_per_pixel = 1.0;
  opts.pixel_width = 2.0;
  vista::TessMesh mesh;
  record(dataset, "dispatch",
         time_us([&] {
           vista::tessellate_polyline(xyz.data(), n, 2, opts, mesh);
         }),
         false);
}

void bench_fill_tess(const char* dataset, int n) {
  OGRPolygon* poly = make_ogr_poly(n, 100.0);
  vista::FillTessOptions opts;
  opts.world_units_per_pixel = 0.5;
  opts.max_fan_verts = 512;
  vista::TessMesh mesh;
  record(dataset, "dispatch",
         time_us([&] { vista::tessellate_geometry(poly, opts, mesh); }), false);
  delete poly;
}

void bench_geoms_batch(const char* dataset, size_t count) {
  std::vector<OGRPolygon*> owned;
  owned.reserve(count);
  std::vector<const OGRGeometry*> geoms;
  geoms.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    owned.push_back(make_ogr_poly(96, 20.0 + static_cast<double>(i % 7)));
    geoms.push_back(owned.back());
  }

  const vista::FillTessOptions opts;
  auto serial = [&](vista::TessMesh& out) {
    vista::detail::reset_mesh(out);
    for (size_t i = 0; i < count; ++i) {
      vista::TessMesh part;
      vista::tessellate_geometry(geoms[i], opts, part);
      vista::detail::append_mesh(out, part);
    }
  };
  auto parallel = [&](vista::TessMesh& out) {
    vista::detail::reset_mesh(out);
    std::vector<vista::TessMesh> parts(count);
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, size_t{0}, count, [&](size_t i) {
      vista::detail::clear_tessellate_tls_scratch();
      vista::tessellate_geometry(geoms[i], opts, parts[i]);
    });
    for (size_t i = 0; i < count; ++i) {
      vista::detail::append_mesh(out, parts[i]);
    }
  };

  vista::TessMesh mesh;
  record(dataset, "serial", time_us([&] { serial(mesh); }, 8), false);
  record(dataset, "parallel", time_us([&] { parallel(mesh); }, 8), false);
  record(dataset, "dispatch",
         time_us([&] { vista::tessellate_geoms(geoms.data(), count, mesh); },
                 8),
         false);

  for (OGRPolygon* p : owned) {
    delete p;
  }
}

void bench_cloud(const char* dataset, size_t n) {
  const auto xyz = make_cloud(n);
  vista::TessMesh mesh;
  // Public API auto-picks serial/parallel by threshold.
  const char* scheme =
      n >= vista::detail::kMeshParallelMinCloudPoints ? "parallel" : "serial";
  record(dataset, scheme,
         time_us([&] { vista::tessellate_point_cloud(xyz.data(), n, 0.5f, mesh); },
                 8),
         false);

  // Explicit serial baseline via private threshold bypass: call with chunked
  // single-threaded emission by using half_extent and forcing small path —
  // for fair matrix, also time a hand serial when n is large.
  if (n >= vista::detail::kMeshParallelMinCloudPoints) {
    record(dataset, "serial",
           time_us(
               [&] {
                 vista::TessMesh local;
                 local.positions.reserve(n * 24u);
                 local.indices.reserve(n * 36u);
                 for (size_t i = 0; i < n; ++i) {
                   const float x = xyz[i * 3];
                   const float y = xyz[i * 3 + 1];
                   const float z = xyz[i * 3 + 2];
                   const uint32_t base =
                       static_cast<uint32_t>(local.positions.size() / 3);
                   const float h = 0.5f;
                   const float c[8][3] = {
                       {x - h, y - h, z - h}, {x + h, y - h, z - h},
                       {x + h, y + h, z - h}, {x - h, y + h, z - h},
                       {x - h, y - h, z + h}, {x + h, y - h, z + h},
                       {x + h, y + h, z + h}, {x - h, y + h, z + h},
                   };
                   for (int k = 0; k < 8; ++k) {
                     local.positions.push_back(c[k][0]);
                     local.positions.push_back(c[k][1]);
                     local.positions.push_back(c[k][2]);
                   }
                   const uint32_t faces[12][3] = {
                       {0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6},
                       {0, 4, 5}, {0, 5, 1}, {1, 5, 6}, {1, 6, 2},
                       {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0},
                   };
                   for (int f = 0; f < 12; ++f) {
                     local.indices.push_back(base + faces[f][0]);
                     local.indices.push_back(base + faces[f][1]);
                     local.indices.push_back(base + faces[f][2]);
                   }
                 }
               },
               8),
           false);
  }
}

double find_us(const char* dataset, const char* scheme) {
  for (const Cell& c : g_cells) {
    if (!c.skip && std::strcmp(c.dataset, dataset) == 0 &&
        std::strcmp(c.scheme, scheme) == 0) {
      return c.us;
    }
  }
  return -1.0;
}

void print_kernel_matrix(const std::vector<const char*>& datasets) {
  std::printf("\n## Kernel matrix (us/iter, lower better)\n\n");
  std::printf("| dataset | scalar | avx2 | dispatch | avx2/scalar |\n");
  std::printf("|---|---:|---:|---:|---:|\n");
  for (const char* ds : datasets) {
    const double s = find_us(ds, "scalar");
    const double a = find_us(ds, "avx2");
    const double d = find_us(ds, "dispatch");
    std::printf("| %s | ", ds);
    if (s < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", s);
    }
    if (a < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", a);
    }
    if (d < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", d);
    }
    if (s > 0 && a > 0) {
      std::printf("%.2fx |\n", s / a);
    } else {
      std::printf("— |\n");
    }
  }
}

void print_tess_matrix(const std::vector<const char*>& datasets) {
  std::printf("\n## Tess / batch matrix (us/iter, lower better)\n\n");
  std::printf("| dataset | serial | parallel | dispatch | parallel/serial |\n");
  std::printf("|---|---:|---:|---:|---:|\n");
  for (const char* ds : datasets) {
    const double s = find_us(ds, "serial");
    const double p = find_us(ds, "parallel");
    const double d = find_us(ds, "dispatch");
    std::printf("| %s | ", ds);
    if (s < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", s);
    }
    if (p < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", p);
    }
    if (d < 0) {
      std::printf("— | ");
    } else {
      std::printf("%.2f | ", d);
    }
    if (s > 0 && p > 0) {
      std::printf("%.2fx |\n", s / p);
    } else {
      std::printf("— |\n");
    }
  }
}

}  // namespace

int main() {
  const bool avx2 = vista::detail::mesh_simd_avx2_runtime();
  std::printf("mesh_simd_benchmark: avx2=%s compiled=%s\n",
              avx2 ? "on" : "off",
              vista::detail::mesh_simd_avx2_compiled() ? "yes" : "no");

  // Correctness smoke: extrema scalar vs dispatch on a small ring.
  {
    std::vector<double> x, y;
    make_ring_soa(128, 10.0, &x, &y);
    int sn, ss, se, sw, dn, ds, de, dw;
    vista::detail::find_xy_extrema_scalar(x.data(), y.data(), 128, &sn, &ss,
                                         &se, &sw);
    vista::detail::find_xy_extrema(x.data(), y.data(), 128, &dn, &ds, &de, &dw);
    if (sn != dn || ss != ds || se != de || sw != dw) {
      std::fprintf(stderr, "extrema mismatch scalar vs dispatch\n");
      return 1;
    }
  }

  std::vector<const char*> kernel_ds;
  std::vector<const char*> tess_ds;

  {
    std::vector<double> x, y;
    make_ring_soa(2048, 100.0, &x, &y);
    bench_extrema("extrema_2k", x, y, avx2);
    kernel_ds.push_back("extrema_2k");
    make_ring_soa(16384, 100.0, &x, &y);
    bench_extrema("extrema_16k", x, y, avx2);
    kernel_ds.push_back("extrema_16k");
  }
  {
    std::vector<double> x, y;
    make_ring_soa(4096, 100.0, &x, &y);
    bench_rdp("rdp_4k", x, y, avx2);
    kernel_ds.push_back("rdp_4k");
    make_ring_soa(32768, 100.0, &x, &y);
    bench_rdp("rdp_32k", x, y, avx2);
    kernel_ds.push_back("rdp_32k");
  }
  bench_normalize("dirs_1k", 1024, avx2);
  kernel_ds.push_back("dirs_1k");
  bench_normalize("dirs_16k", 16384, avx2);
  kernel_ds.push_back("dirs_16k");
  {
    std::vector<double> x, y;
    make_ring_soa(4096, 50.0, &x, &y);
    bench_path("path_4k", x, y, avx2);
    kernel_ds.push_back("path_4k");
    make_ring_soa(65536, 50.0, &x, &y);
    bench_path("path_64k", x, y, avx2);
    kernel_ds.push_back("path_64k");
  }
  {
    auto c = make_cloud(4096);
    bench_aabb("aabb_4k", c, avx2);
    kernel_ds.push_back("aabb_4k");
    c = make_cloud(65536);
    bench_aabb("aabb_64k", c, avx2);
    kernel_ds.push_back("aabb_64k");
  }

  bench_line_tess("line_poly_512", 512);
  tess_ds.push_back("line_poly_512");
  bench_line_tess("line_poly_4k", 4096);
  tess_ds.push_back("line_poly_4k");
  bench_fill_tess("fill_ring_2k", 2048);
  tess_ds.push_back("fill_ring_2k");
  bench_fill_tess("fill_ring_8k", 8192);
  tess_ds.push_back("fill_ring_8k");
  bench_geoms_batch("geoms_64", 64);
  tess_ds.push_back("geoms_64");
  bench_geoms_batch("geoms_512", 512);
  tess_ds.push_back("geoms_512");
  bench_geoms_batch("geoms_1024", 1024);
  tess_ds.push_back("geoms_1024");
  bench_cloud("cloud_1k", 1024);
  tess_ds.push_back("cloud_1k");
  bench_cloud("cloud_4k", 4096);
  tess_ds.push_back("cloud_4k");

  print_kernel_matrix(kernel_ds);
  print_tess_matrix(tess_ds);

  std::printf(
      "\nNotes: kernel schemes are scalar / avx2 / dispatch. Tess schemes are "
      "serial / parallel / dispatch (full public path). Speedup >1 means "
      "faster.\n");
  return 0;
}
