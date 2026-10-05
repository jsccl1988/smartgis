// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/idw_profile.h"
#include "gis/analysis/geochem/samples.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct BenchRow {
  int n_pts = 0;
  int cell_count = 0;
  int width = 0;
  int height = 0;
  double wall_ms = 0;
  double mpx_s = 0;
  double work_rate = 0;
  const char* backend = "serial";
};

gis::detail::GeochemSampleSet make_square_samples(int n) {
  gis::detail::GeochemSampleSet set;
  set.ok = true;
  set.element_names.emplace_back("Cu");
  set.samples.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    gis::detail::GeochemSample s;
    s.id = std::to_string(i);
    // Diagonal in a unit-span box so padded aspect stays ~1 (W≈H≈cell_count).
    s.x = static_cast<double>(i);
    s.y = static_cast<double>(i);
    s.values.push_back(10.0 + 0.25 * static_cast<double>(i));
    set.samples.push_back(std::move(s));
  }
  return set;
}

double median_ms(std::vector<double>* samples) {
  if (!samples || samples->empty()) {
    return 0;
  }
  std::sort(samples->begin(), samples->end());
  const size_t n = samples->size();
  if (n % 2u == 1u) {
    return (*samples)[n / 2u];
  }
  return 0.5 * ((*samples)[n / 2u - 1u] + (*samples)[n / 2u]);
}

BenchRow run_case(int n_pts, int cell_count) {
  const auto set = make_square_samples(n_pts);
  // Throwaway warm-up (not timed).
  (void)gis::detail::run_geochem_idw(set, "Cu", cell_count, 2.0,
                                     std::numeric_limits<double>::quiet_NaN(),
                                     2.0);

  std::vector<double> walls;
  walls.reserve(3);
  gis::detail::GeochemIdwResult last;
  for (int rep = 0; rep < 3; ++rep) {
    const auto t0 = std::chrono::steady_clock::now();
    last = gis::detail::run_geochem_idw(set, "Cu", cell_count, 2.0,
                                        std::numeric_limits<double>::quiet_NaN(),
                                        2.0);
    const auto t1 = std::chrono::steady_clock::now();
    walls.push_back(gis::detail::geochem_idw_elapsed_ms(t0, t1));
  }

  BenchRow row;
  row.n_pts = n_pts;
  row.cell_count = cell_count;
  row.width = last.width;
  row.height = last.height;
  row.wall_ms = median_ms(&walls);
  const double pixels =
      static_cast<double>(last.width) * static_cast<double>(last.height);
  const double wall_s = row.wall_ms / 1000.0;
  row.mpx_s = wall_s > 0.0 ? (pixels / 1.0e6) / wall_s : 0.0;
  row.work_rate =
      wall_s > 0.0 ? (pixels * static_cast<double>(n_pts)) / wall_s : 0.0;
  const long long cells =
      static_cast<long long>(last.width) * static_cast<long long>(last.height);
  row.backend = (cells >= 4096 && last.height >= 8) ? "parallel" : "serial";
  return row;
}

std::filesystem::path exe_dir(const char* argv0) {
  std::error_code ec;
  std::filesystem::path p(argv0 ? argv0 : "");
  if (p.empty()) {
    return std::filesystem::current_path(ec);
  }
  p = std::filesystem::absolute(p, ec);
  if (ec) {
    return std::filesystem::current_path();
  }
  return p.parent_path();
}

bool write_text(const std::filesystem::path& path, const std::string& body) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << body;
  return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  _putenv_s("ANALYSIS_PROFILE", "1");
#else
  setenv("ANALYSIS_PROFILE", "1", 1);
#endif

  const int ns[] = {10, 50, 200};
  const int grids[] = {32, 64, 256, 512};

  std::string md;
  md += "# geochem IDW bench\n\n";
  md += "Gates: `parallel_for` when `W*H >= 4096` and `H >= 8`. "
        "32x32 stays serial; 64+ is parallel. Inner sample loop is per-pixel. "
        "Default product `cell_count` remains 64.\n\n";
  md += "| N_pts | cells | W | H | backend | wall_ms | Mpx/s | "
        "points×pixels/s |\n";
  md += "| --- | --- | --- | --- | --- | --- | --- | --- |\n";

  std::string json = "{\n  \"cases\": [\n";
  bool first_json = true;

  for (int n : ns) {
    for (int cells : grids) {
      const BenchRow r = run_case(n, cells);
      char line[512];
      std::snprintf(line, sizeof(line),
                    "N=%d cells=%d W=%d H=%d backend=%s wall_ms=%.3f "
                    "Mpx/s=%.3f work_rate=%.3e\n",
                    r.n_pts, r.cell_count, r.width, r.height, r.backend,
                    r.wall_ms, r.mpx_s, r.work_rate);
      std::fputs(line, stdout);
      char md_line[512];
      std::snprintf(md_line, sizeof(md_line),
                    "| %d | %d | %d | %d | %s | %.3f | %.3f | %.3e |\n", r.n_pts,
                    r.cell_count, r.width, r.height, r.backend, r.wall_ms,
                    r.mpx_s, r.work_rate);
      md += md_line;
      if (!first_json) {
        json += ",\n";
      }
      first_json = false;
      char js[640];
      std::snprintf(js, sizeof(js),
                    "    {\"n_pts\":%d,\"cells\":%d,\"W\":%d,\"H\":%d,"
                    "\"backend\":\"%s\",\"wall_ms\":%.6f,\"mpx_s\":%.6f,"
                    "\"work_rate\":%.6e}",
                    r.n_pts, r.cell_count, r.width, r.height, r.backend,
                    r.wall_ms, r.mpx_s, r.work_rate);
      json += js;
    }
  }
  json += "\n  ]\n}\n";
  md += "\nThresholds: tests use cells 24/32 (serial). Nodata stays -9999; "
        "`dist2 < 1e-18` takes the sample value; power default 2.\n";

  const std::filesystem::path root = exe_dir(argc > 0 ? argv[0] : nullptr);
  const std::filesystem::path log_path = root / "log" / "geochem_idw_bench.txt";
  const std::filesystem::path cap_dir =
      root / "captures" / "analysis" / "idw";
  write_text(log_path, md);
  write_text(cap_dir / "idw_bench.md", md);
  write_text(cap_dir / "idw_bench.json", json);
  std::fprintf(stdout, "wrote %s\n", log_path.string().c_str());
  std::fprintf(stdout, "wrote %s\n", (cap_dir / "idw_bench.md").string().c_str());
  return 0;
}
