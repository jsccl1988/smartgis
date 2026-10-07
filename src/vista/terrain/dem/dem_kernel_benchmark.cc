// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Dataset × scheme performance matrix for vista/terrain DEM kernels.
//
//   out\Debug\dem_kernel_benchmark.exe
// Artifacts:
//   out/<config>/captures/analysis/terrain_kernel/matrix.json
//   out/<config>/captures/analysis/terrain_kernel/matrix.md
//   out/<config>/log/dem_kernel_matrix.json

#include "vista/terrain/dem/bake/bake_backend.h"
#include "vista/terrain/dem/bake/bake_parallel.h"
#include "vista/terrain/dem/detail/dem_simd.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "vista/terrain/dem/shade/dem_hillshade.h"
#include "vista/terrain/dem/shade/lit_kern.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

enum class DatasetKind {
  kSynth256 = 0,
  kSynth512,
  kSynth1024,
  kChina256,
  kChina512,
  kChina768,
};

enum class SchemeKind {
  kSerialScalar = 0,
  kParallelScalar,
  kParallelSimd,
  kCuda,
};

enum class KernelKind {
  kShade = 0,
  kMesh,
  kHypso,
  kLodFill,
  kPackLambert,
};

struct CaseResult {
  const char* dataset = "";
  const char* scheme = "";
  const char* kernel = "";
  int src_w = 0;
  int src_h = 0;
  int out_w = 0;
  int out_h = 0;
  double wall_ms = 0.0;
  double mpx_s = 0.0;
  int ok = 0;
  int used_cuda = 0;
  int used_simd = 0;
  int parallel = 0;
};

const char* dataset_name(DatasetKind d) {
  switch (d) {
    case DatasetKind::kSynth256:
      return "synth_256";
    case DatasetKind::kSynth512:
      return "synth_512";
    case DatasetKind::kSynth1024:
      return "synth_1024";
    case DatasetKind::kChina256:
      return "china_e256";
    case DatasetKind::kChina512:
      return "china_e512";
    case DatasetKind::kChina768:
      return "china_e768";
  }
  return "?";
}

const char* scheme_name(SchemeKind s) {
  switch (s) {
    case SchemeKind::kSerialScalar:
      return "serial_scalar";
    case SchemeKind::kParallelScalar:
      return "parallel_scalar";
    case SchemeKind::kParallelSimd:
      return "parallel_simd";
    case SchemeKind::kCuda:
      return "cuda";
  }
  return "?";
}

const char* kernel_name(KernelKind k) {
  switch (k) {
    case KernelKind::kShade:
      return "shade_dem_rgba";
    case KernelKind::kMesh:
      return "build_mesh";
    case KernelKind::kHypso:
      return "bake_hypsometric";
    case KernelKind::kLodFill:
      return "fill_lod_bilinear";
    case KernelKind::kPackLambert:
      return "pack_lambert";
  }
  return "?";
}

int dataset_max_edge(DatasetKind d) {
  switch (d) {
    case DatasetKind::kSynth256:
    case DatasetKind::kChina256:
      return 256;
    case DatasetKind::kSynth512:
    case DatasetKind::kChina512:
      return 512;
    case DatasetKind::kSynth1024:
    case DatasetKind::kChina768:
      return 768;
  }
  return 256;
}

int dataset_synth_edge(DatasetKind d) {
  switch (d) {
    case DatasetKind::kSynth256:
      return 256;
    case DatasetKind::kSynth512:
      return 512;
    case DatasetKind::kSynth1024:
      return 1024;
    default:
      return 0;
  }
}

bool is_china(DatasetKind d) {
  return d == DatasetKind::kChina256 || d == DatasetKind::kChina512 ||
         d == DatasetKind::kChina768;
}

double median_of(std::vector<double> samples) {
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const size_t n = samples.size();
  if (n % 2u == 1u) {
    return samples[n / 2u];
  }
  return 0.5 * (samples[n / 2u - 1u] + samples[n / 2u]);
}

void apply_scheme(SchemeKind scheme) {
  switch (scheme) {
    case SchemeKind::kSerialScalar:
      vista::set_bake_parallel_override(vista::kBakeDispatchOff);
      vista::set_bake_simd_override(vista::kBakeDispatchOff);
      _putenv_s("BAKE_PARALLEL", "0");
      _putenv_s("BAKE_SIMD", "0");
      _putenv_s("BAKE_BACKEND", "cpu");
      break;
    case SchemeKind::kParallelScalar:
      vista::set_bake_parallel_override(vista::kBakeDispatchOn);
      vista::set_bake_simd_override(vista::kBakeDispatchOff);
      _putenv_s("BAKE_PARALLEL", "1");
      _putenv_s("BAKE_SIMD", "0");
      _putenv_s("BAKE_BACKEND", "cpu");
      break;
    case SchemeKind::kParallelSimd:
      vista::set_bake_parallel_override(vista::kBakeDispatchOn);
      vista::set_bake_simd_override(vista::kBakeDispatchOn);
      _putenv_s("BAKE_PARALLEL", "1");
      _putenv_s("BAKE_SIMD", "1");
      _putenv_s("BAKE_BACKEND", "cpu");
      break;
    case SchemeKind::kCuda:
      vista::set_bake_parallel_override(vista::kBakeDispatchAuto);
      vista::set_bake_simd_override(vista::kBakeDispatchAuto);
      _putenv_s("BAKE_PARALLEL", "1");
      _putenv_s("BAKE_SIMD", "1");
      _putenv_s("BAKE_BACKEND", "cuda");
      break;
  }
}

void reset_scheme() {
  vista::set_bake_parallel_override(vista::kBakeDispatchAuto);
  vista::set_bake_simd_override(vista::kBakeDispatchAuto);
  _putenv_s("BAKE_PARALLEL", "1");
  _putenv_s("BAKE_SIMD", "1");
  _putenv_s("BAKE_BACKEND", "auto");
}

vista::DemRaster make_synth(int edge) {
  vista::DemRaster dem;
  // adopt_bake_cache: synthetic ramp without GDAL.
  std::vector<float> heights(static_cast<size_t>(edge) * static_cast<size_t>(edge));
  std::vector<uint8_t> land(heights.size(), 1);
  for (int row = 0; row < edge; ++row) {
    for (int col = 0; col < edge; ++col) {
      const float h =
          50.f + 1200.f * (static_cast<float>(col) / static_cast<float>(edge)) +
          800.f * (static_cast<float>(row) / static_cast<float>(edge));
      heights[static_cast<size_t>(row) * static_cast<size_t>(edge) +
              static_cast<size_t>(col)] = h;
    }
  }
  dem.adopt_bake_cache(edge, edge, 100.0, 20.0, 120.0, 40.0, 50.f, 2050.f,
                       0.0012f, std::move(heights), std::move(land),
                       "synth_bench");
  return dem;
}

bool load_china(vista::DemRaster* dem) {
  const std::string path = vista::find_sample_dem_path();
  if (path.empty() || !dem->load_gdal_raster(path.c_str()) || dem->empty()) {
    return false;
  }
  return true;
}

std::filesystem::path artifact_root() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    std::filesystem::path p(module);
    return p.parent_path();
  }
  return std::filesystem::current_path();
}

CaseResult run_case(const vista::DemRaster& dem, DatasetKind dataset,
                    SchemeKind scheme, KernelKind kernel) {
  using Clock = std::chrono::steady_clock;
  CaseResult out;
  out.dataset = dataset_name(dataset);
  out.scheme = scheme_name(scheme);
  out.kernel = kernel_name(kernel);
  out.src_w = dem.cols();
  out.src_h = dem.rows();
  out.parallel = (scheme == SchemeKind::kSerialScalar) ? 0 : 1;
  out.used_simd = (scheme == SchemeKind::kParallelSimd) ? 1 : 0;

  apply_scheme(scheme);
  const int max_edge = dataset_max_edge(dataset);
  constexpr int kReps = 5;

  auto time_ms = [&](auto&& fn) -> double {
    fn();  // warmup
    std::vector<double> walls;
    walls.reserve(static_cast<size_t>(kReps));
    for (int i = 0; i < kReps; ++i) {
      const auto t0 = Clock::now();
      fn();
      const auto t1 = Clock::now();
      walls.push_back(
          std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return median_of(std::move(walls));
  };

  if (kernel == KernelKind::kShade) {
    vista::HillshadeParams hs;
    hs.max_edge = max_edge;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    vista::reset_last_shade_used_cuda();
    out.wall_ms = time_ms([&]() {
      rgba.clear();
      out.ok = vista::shade_dem_rgba(dem, hs, &rgba, &w, &h) ? 1 : 0;
      out.out_w = w;
      out.out_h = h;
    });
    out.used_cuda = vista::last_shade_used_cuda();
    if (scheme == SchemeKind::kCuda && !out.ok) {
      out.ok = 0;
    }
  } else if (kernel == KernelKind::kMesh) {
    std::vector<float> xyz;
    std::vector<uint32_t> idx;
    out.wall_ms = time_ms([&]() {
      xyz.clear();
      idx.clear();
      out.ok = dem.build_mesh(max_edge, &xyz, &idx) ? 1 : 0;
      out.out_w = max_edge;
      out.out_h = max_edge;
    });
  } else if (kernel == KernelKind::kHypso) {
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    out.wall_ms = time_ms([&]() {
      rgba.clear();
      out.ok = dem.bake_hypsometric_rgba(max_edge, &rgba, &w, &h) ? 1 : 0;
      out.out_w = w;
      out.out_h = h;
    });
  } else if (kernel == KernelKind::kLodFill) {
    const int w = max_edge;
    const int h = max_edge;
    std::vector<float> lod(static_cast<size_t>(w) * static_cast<size_t>(h));
    const float* heights = dem.heights_data();
    out.wall_ms = time_ms([&]() {
      if (!heights) {
        out.ok = 0;
        return;
      }
      vista::detail::fill_lod_bilinear_grid(heights, dem.cols(), dem.rows(), w,
                                            h, lod.data());
      out.ok = 1;
      out.out_w = w;
      out.out_h = h;
    });
    out.used_simd = vista::detail::dem_simd_avx2_runtime() ? 1 : 0;
  } else if (kernel == KernelKind::kPackLambert) {
    const size_t n = static_cast<size_t>(max_edge) * static_cast<size_t>(max_edge);
    std::vector<float> shade(n);
    std::vector<float> heights(n);
    std::vector<uint8_t> rgba(n * 4u);
    for (size_t i = 0; i < n; ++i) {
      shade[i] = 0.2f + 0.6f * (static_cast<float>(i % 97u) / 97.f);
      heights[i] = 100.f + static_cast<float>(i % 50u);
    }
    out.wall_ms = time_ms([&]() {
      vista::detail::pack_lambert_from_shade(
          shade.data(), heights.data(), rgba.data(), n, 0.1f, 0.1f, 0.15f, 0.9f,
          0.85f, 0.7f, true);
      out.ok = 1;
      out.out_w = max_edge;
      out.out_h = max_edge;
    });
    out.used_simd = vista::detail::dem_lit_avx2_runtime() ? 1 : 0;
  }

  reset_scheme();
  const double pixels =
      static_cast<double>((std::max)(1, out.out_w)) *
      static_cast<double>((std::max)(1, out.out_h));
  out.mpx_s = (out.wall_ms > 0.0) ? (pixels / (out.wall_ms * 1000.0)) : 0.0;
  return out;
}

void write_json(const std::filesystem::path& path,
                const std::vector<CaseResult>& rows) {
  std::filesystem::create_directories(path.parent_path());
  FILE* f = nullptr;
  fopen_s(&f, path.string().c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "WARN: cannot write %s\n", path.string().c_str());
    return;
  }
  std::fprintf(f,
               "{\n  \"note\": \"median of %d reps after 1 warmup; "
               "cuda ok=0 means skip (no device)\",\n"
               "  \"simd_runtime\": %s,\n"
               "  \"cases\": [\n",
               5, vista::detail::dem_simd_avx2_runtime() ? "true" : "false");
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto& r = rows[i];
    std::fprintf(f,
                 "    {\"dataset\":\"%s\",\"scheme\":\"%s\",\"kernel\":\"%s\","
                 "\"src_w\":%d,\"src_h\":%d,\"out_w\":%d,\"out_h\":%d,"
                 "\"wall_ms\":%.4f,\"mpx_s\":%.4f,\"ok\":%d,\"used_cuda\":%d,"
                 "\"used_simd\":%d,\"parallel\":%d}%s\n",
                 r.dataset, r.scheme, r.kernel, r.src_w, r.src_h, r.out_w,
                 r.out_h, r.wall_ms, r.mpx_s, r.ok, r.used_cuda, r.used_simd,
                 r.parallel, (i + 1 < rows.size()) ? "," : "");
  }
  std::fprintf(f, "  ]\n}\n");
  std::fclose(f);
  std::printf("wrote %s\n", path.string().c_str());
}

void write_md(const std::filesystem::path& path,
              const std::vector<CaseResult>& rows) {
  std::filesystem::create_directories(path.parent_path());
  FILE* f = nullptr;
  fopen_s(&f, path.string().c_str(), "wb");
  if (!f) {
    return;
  }
  std::fprintf(f,
               "# vista/terrain DEM kernel matrix\n\n"
               "Schemes: serial_scalar | parallel_scalar | parallel_simd | "
               "cuda.\n"
               "Datasets: synth_{256,512,1024} + china_dem @ max_edge "
               "{256,512,768}.\n\n"
               "| dataset | scheme | kernel | wall_ms | Mpx/s | ok | cuda | "
               "simd | out |\n"
               "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |\n");
  for (const auto& r : rows) {
    std::fprintf(f,
                 "| %s | %s | %s | %.3f | %.3f | %d | %d | %d | %dx%d |\n",
                 r.dataset, r.scheme, r.kernel, r.wall_ms, r.mpx_s, r.ok,
                 r.used_cuda, r.used_simd, r.out_w, r.out_h);
  }
  std::fclose(f);
}

}  // namespace

int main() {
  _putenv_s("BAKE_DISK", "0");
  _putenv_s("BAKE_BENCH", "1");

  vista::DemRaster china;
  const bool have_china = load_china(&china);
  if (!have_china) {
    std::fprintf(stderr,
                 "dem_kernel_benchmark: china_dem missing — synth cells only "
                 "(build //testing/data:china_map_samples)\n");
  }

  const DatasetKind datasets[] = {
      DatasetKind::kSynth256, DatasetKind::kSynth512, DatasetKind::kSynth1024,
      DatasetKind::kChina256, DatasetKind::kChina512, DatasetKind::kChina768,
  };
  const SchemeKind schemes[] = {
      SchemeKind::kSerialScalar, SchemeKind::kParallelScalar,
      SchemeKind::kParallelSimd, SchemeKind::kCuda,
  };
  const KernelKind kernels[] = {
      KernelKind::kShade, KernelKind::kMesh, KernelKind::kHypso,
      KernelKind::kLodFill, KernelKind::kPackLambert,
  };

  std::vector<CaseResult> rows;
  rows.reserve(120);

  std::printf(
      "vista/terrain DEM matrix  (median of 5 after warmup)\n"
      "simd_runtime=%d lit_avx2=%d\n\n",
      vista::detail::dem_simd_avx2_runtime() ? 1 : 0,
      vista::detail::dem_lit_avx2_runtime() ? 1 : 0);
  std::printf("%-12s %-16s %-18s %10s %10s\n", "dataset", "scheme", "kernel",
              "wall_ms", "Mpx/s");

  for (DatasetKind ds : datasets) {
    if (is_china(ds) && !have_china) {
      continue;
    }
    vista::DemRaster local;
    const vista::DemRaster* dem = &china;
    if (!is_china(ds)) {
      local = make_synth(dataset_synth_edge(ds));
      dem = &local;
    }
    for (SchemeKind scheme : schemes) {
      for (KernelKind kernel : kernels) {
        // pack_lambert / lod are CPU kernels — skip redundant cuda cells
        // (same as parallel_simd timing) except shade which has a Thrust path.
        if (scheme == SchemeKind::kCuda &&
            (kernel == KernelKind::kPackLambert ||
             kernel == KernelKind::kLodFill || kernel == KernelKind::kMesh)) {
          continue;
        }
        const CaseResult r = run_case(*dem, ds, scheme, kernel);
        rows.push_back(r);
        std::printf("%-12s %-16s %-18s %10.3f %10.3f%s\n", r.dataset, r.scheme,
                    r.kernel, r.wall_ms, r.mpx_s,
                    r.ok ? "" : "  (skip/fail)");
      }
    }
  }

  const auto root = artifact_root();
  write_json(root / "log" / "dem_kernel_matrix.json", rows);
  write_json(root / "captures" / "analysis" / "terrain_kernel" / "matrix.json",
             rows);
  write_md(root / "captures" / "analysis" / "terrain_kernel" / "matrix.md",
           rows);

  // Print a compact shade pivot table (dataset × scheme).
  std::printf("\n## shade_dem_rgba pivot (wall_ms)\n");
  std::printf("%-12s", "dataset");
  for (SchemeKind s : schemes) {
    std::printf(" %14s", scheme_name(s));
  }
  std::printf("\n");
  for (DatasetKind ds : datasets) {
    if (is_china(ds) && !have_china) {
      continue;
    }
    std::printf("%-12s", dataset_name(ds));
    for (SchemeKind s : schemes) {
      double ms = -1.0;
      for (const auto& r : rows) {
        if (std::strcmp(r.dataset, dataset_name(ds)) == 0 &&
            std::strcmp(r.scheme, scheme_name(s)) == 0 &&
            std::strcmp(r.kernel, "shade_dem_rgba") == 0) {
          ms = r.ok ? r.wall_ms : -1.0;
          break;
        }
      }
      if (ms < 0.0) {
        std::printf(" %14s", "-");
      } else {
        std::printf(" %14.3f", ms);
      }
    }
    std::printf("\n");
  }

  return 0;
}
