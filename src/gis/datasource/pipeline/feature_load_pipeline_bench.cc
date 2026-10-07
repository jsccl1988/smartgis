// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Dataset × FeatureLoadOptions matrix for load_ogr_layer_pipeline.
// Timed decode uses load_ogr_feature_parts (geom ring sample + field UTF-8),
// matching product ingest cost. Asserts loaded feature count vs a serial
// reference so high-worker drain races fail the bench loudly.
//
// Usage:
//   set SMARTGIS_ROOT=<repo>
//   feature_load_pipeline_bench.exe
//   feature_load_pipeline_bench.exe --stress 30
//   feature_load_pipeline_bench.exe --geom-only   # skip field UTF-8 (A/B)

#include "gis/datasource/pipeline/feature_load_pipeline.h"
#include "gis/datasource/pipeline/ogr_feature_load.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

namespace {

namespace fs = std::filesystem;

struct Scheme {
  const char* name = nullptr;
  gis::datasource::FeatureLoadOptions opts;
  // When true, call load_ogr_layer_serial directly. Needed because GeoJSON
  // often returns GetFeatureCount(FALSE) == -1, so serial_threshold alone
  // cannot select the serial path.
  bool force_serial = false;
};

struct DatasetSpec {
  const char* label = nullptr;
  const char* relative = nullptr;
};

struct CellResult {
  bool ok = false;
  bool count_ok = false;
  size_t features = 0;
  size_t parts = 0;
  size_t field_values = 0;
  double wall_ms = 0;
  double feat_per_s = 0;
};

constexpr int kWarmup = 1;
constexpr int kTimedRuns = 5;

const DatasetSpec kDatasets[] = {
    {"china_plp", "testing/data/china/china_plp.geojson"},
    {"china_roads", "testing/data/china/china_roads.src.geojson"},
    {"china_hydro", "testing/data/china/china_hydro.src.geojson"},
    {"china_city_geojson", "testing/data/china/china_city.geojson"},
    {"china_city_gpkg", "testing/data/china/china_city.gpkg"},
    {"ne_outline", "testing/data/china/_china_ne_outline.geojson"},
};

bool g_geom_only = false;

std::vector<Scheme> make_schemes() {
  using gis::datasource::FeatureLoadOptions;
  std::vector<Scheme> out;

  {
    FeatureLoadOptions o;
    o.decode_workers = 1;
    o.ordered_window = 256;
    out.push_back({"serial", o, /*force_serial=*/true});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 1;
    o.ordered_window = 256;
    out.push_back({"pipe_w1_win256", o});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 2;
    o.ordered_window = 256;
    out.push_back({"pipe_w2_win256", o});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 4;
    o.ordered_window = 256;
    out.push_back({"pipe_w4_win256", o});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 8;
    o.ordered_window = 256;
    out.push_back({"pipe_w8_win256", o});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 4;
    o.ordered_window = 0;
    out.push_back({"pipe_w4_win0", o});
  }
  {
    FeatureLoadOptions o;
    o.serial_threshold = 0;
    o.decode_workers = 4;
    o.ordered_window = 32;
    out.push_back({"pipe_w4_win32", o});
  }
  {
    out.push_back({"product_defaults", FeatureLoadOptions{}});
  }
  return out;
}

bool file_exists(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

std::string resolve_dataset(const char* relative) {
  std::vector<fs::path> roots;
  roots.push_back(fs::current_path());
  if (const char* env = std::getenv("SMARTGIS_ROOT")) {
    if (env[0]) {
      roots.emplace_back(env);
    }
  }
  roots.push_back(fs::current_path() / ".." / "..");
  roots.push_back(fs::current_path() / "..");

  for (const fs::path& root : roots) {
    const fs::path candidate = root / relative;
    if (file_exists(candidate)) {
      return fs::weakly_canonical(candidate).string();
    }
  }
  return {};
}

bool skip_text_layer(const char* lname) {
  return lname &&
         (std::strcmp(lname, "text") == 0 || std::strcmp(lname, "Text") == 0);
}

// Geom-only decode: ring sample without field UTF-8 (A/B via --geom-only).
bool decode_feature_geom_only(
    OGRFeature* feat, std::vector<gis::datasource::OgrFeaturePart>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (!feat) {
    return false;
  }
  OGRGeometry* geom = feat->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return false;
  }
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  auto push_ring = [&](OGRLineString* ring, gis::datasource::OgrPartKind kind) {
    gis::datasource::OgrFeaturePart part;
    part.kind = kind;
    gis::datasource::decimate_ogr_ring(ring, &part.points);
    out->push_back(std::move(part));
  };
  if (flat == wkbPoint) {
    auto* pt = geom->toPoint();
    gis::datasource::OgrFeaturePart part;
    part.kind = gis::datasource::OgrPartKind::kPoint;
    part.points.push_back({pt->getX(), -pt->getY()});
    out->push_back(std::move(part));
    return true;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    push_ring(geom->toLineString(), gis::datasource::OgrPartKind::kLine);
    return out->front().points.size() >= 2;
  }
  if (flat == wkbPolygon) {
    if (OGRLinearRing* ext = geom->toPolygon()->getExteriorRing()) {
      push_ring(ext, gis::datasource::OgrPartKind::kPolygon);
    }
    return !out->empty() && out->front().points.size() >= 3;
  }
  if (flat == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    if (!multi) {
      return false;
    }
    for (int i = 0; i < multi->getNumGeometries(); ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbLineString) {
        continue;
      }
      push_ring(part->toLineString(), gis::datasource::OgrPartKind::kLine);
    }
    return !out->empty();
  }
  if (flat == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    if (!multi) {
      return false;
    }
    for (int i = 0; i < multi->getNumGeometries(); ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbPolygon) {
        continue;
      }
      if (OGRLinearRing* ext = part->toPolygon()->getExteriorRing()) {
        push_ring(ext, gis::datasource::OgrPartKind::kPolygon);
      }
    }
    return !out->empty();
  }
  if (flat == wkbMultiPoint) {
    auto* multi = geom->toMultiPoint();
    if (!multi || multi->getNumGeometries() < 1) {
      return false;
    }
    auto* pt = multi->getGeometryRef(0)->toPoint();
    gis::datasource::OgrFeaturePart part;
    part.kind = gis::datasource::OgrPartKind::kPoint;
    part.points.push_back({pt->getX(), -pt->getY()});
    out->push_back(std::move(part));
    return true;
  }
  return false;
}

CellResult load_once(GDALDataset* ds, const Scheme& scheme) {
  CellResult r;
  if (!ds) {
    return r;
  }

  size_t features = 0;
  size_t parts = 0;
  size_t field_values = 0;
  const auto t0 = std::chrono::steady_clock::now();

  const int layer_count = ds->GetLayerCount();
  for (int li = 0; li < layer_count; ++li) {
    OGRLayer* layer = ds->GetLayer(li);
    if (!layer) {
      continue;
    }
    if (skip_text_layer(layer->GetName())) {
      continue;
    }

    auto decode =
        [](OGRFeature* feat,
           std::vector<gis::datasource::OgrFeaturePart>* out) {
          return g_geom_only
                     ? decode_feature_geom_only(feat, out)
                     : gis::datasource::load_ogr_feature_parts(feat, out) > 0;
        };
    auto sink =
        [&](std::vector<gis::datasource::OgrFeaturePart>&& decoded) {
          parts += decoded.size();
          for (const auto& p : decoded) {
            field_values += p.fields.size();
          }
          ++features;
        };

    bool ok = false;
    if (scheme.force_serial) {
      ok = gis::datasource::detail::load_ogr_layer_serial<
          std::vector<gis::datasource::OgrFeaturePart>>(
          layer, decode, sink, scheme.opts.max_features);
    } else {
      ok = gis::datasource::load_ogr_layer_pipeline<
          std::vector<gis::datasource::OgrFeaturePart>>(layer, decode, sink,
                                                        scheme.opts);
    }
    if (!ok) {
      return r;
    }
  }

  const auto t1 = std::chrono::steady_clock::now();
  r.ok = true;
  r.features = features;
  r.parts = parts;
  r.field_values = field_values;
  r.wall_ms =
      std::chrono::duration<double, std::milli>(t1 - t0).count();
  if (r.wall_ms > 0.0) {
    r.feat_per_s = (1000.0 * static_cast<double>(r.features)) / r.wall_ms;
  }
  return r;
}

double median_ms(std::vector<double> samples) {
  if (samples.empty()) {
    return 0;
  }
  std::sort(samples.begin(), samples.end());
  const size_t n = samples.size();
  if (n % 2 == 1) {
    return samples[n / 2];
  }
  return 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
}

size_t serial_reference_count(const std::string& path) {
  GDALDataset* ds =
      GDALDataset::Open(path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY);
  if (!ds) {
    return 0;
  }
  Scheme serial;
  serial.name = "ref";
  serial.force_serial = true;
  CellResult r = load_once(ds, serial);
  GDALClose(ds);
  return r.ok ? r.features : 0;
}

CellResult bench_cell(const std::string& path, const Scheme& scheme,
                      size_t expect_features) {
  CellResult r;
  GDALDataset* ds =
      GDALDataset::Open(path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY);
  if (!ds) {
    std::fprintf(stderr, "FAIL open: %s\n", path.c_str());
    return r;
  }

  for (int i = 0; i < kWarmup; ++i) {
    (void)load_once(ds, scheme);
  }

  std::vector<double> times;
  times.reserve(static_cast<size_t>(kTimedRuns));
  size_t features = 0;
  size_t parts = 0;
  size_t field_values = 0;
  bool count_ok = true;
  for (int i = 0; i < kTimedRuns; ++i) {
    CellResult once = load_once(ds, scheme);
    if (!once.ok) {
      GDALClose(ds);
      return r;
    }
    if (once.features != expect_features) {
      std::fprintf(stderr,
                   "COUNT MISMATCH scheme=%s got=%zu expect=%zu run=%d\n",
                   scheme.name, once.features, expect_features, i);
      count_ok = false;
    }
    times.push_back(once.wall_ms);
    features = once.features;
    parts = once.parts;
    field_values = once.field_values;
  }
  GDALClose(ds);

  r.ok = true;
  r.count_ok = count_ok;
  r.features = features;
  r.parts = parts;
  r.field_values = field_values;
  r.wall_ms = median_ms(std::move(times));
  if (r.wall_ms > 0.0) {
    r.feat_per_s = (1000.0 * static_cast<double>(r.features)) / r.wall_ms;
  }
  return r;
}

// Extra integrity-only rounds (no timing matrix) for high-worker schemes.
bool stress_integrity(const std::string& path, const char* label,
                      size_t expect_features, int rounds) {
  const Scheme schemes[] = {
      [] {
        Scheme s;
        s.name = "pipe_w8_win256";
        s.opts.serial_threshold = 0;
        s.opts.decode_workers = 8;
        s.opts.ordered_window = 256;
        return s;
      }(),
      [] {
        Scheme s;
        s.name = "pipe_w8_win32";
        s.opts.serial_threshold = 0;
        s.opts.decode_workers = 8;
        s.opts.ordered_window = 32;
        return s;
      }(),
      [] {
        Scheme s;
        s.name = "pipe_w8_win0";
        s.opts.serial_threshold = 0;
        s.opts.decode_workers = 8;
        s.opts.ordered_window = 0;
        return s;
      }(),
  };

  for (int r = 0; r < rounds; ++r) {
    for (const Scheme& scheme : schemes) {
      GDALDataset* ds =
          GDALDataset::Open(path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY);
      if (!ds) {
        std::fprintf(stderr, "stress open fail %s\n", label);
        return false;
      }
      CellResult once = load_once(ds, scheme);
      GDALClose(ds);
      if (!once.ok || once.features != expect_features) {
        std::fprintf(stderr,
                     "STRESS FAIL %s scheme=%s round=%d got=%zu expect=%zu\n",
                     label, scheme.name, r, once.features, expect_features);
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  GDALAllRegister();

  int stress_rounds = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--geom-only") == 0) {
      g_geom_only = true;
    } else if (std::strcmp(argv[i], "--stress") == 0 && i + 1 < argc) {
      stress_rounds = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::fprintf(stderr,
                   "usage: feature_load_pipeline_bench [--geom-only] "
                   "[--stress N]\n"
                   "  SMARTGIS_ROOT = repo root if cwd is not nearby.\n");
      return 0;
    }
  }

  const char* config_label = "unknown";
#if defined(NDEBUG)
  config_label = "Release";
#else
  config_label = "Debug";
#endif

  const unsigned hw = std::thread::hardware_concurrency();
  std::printf(
      "feature_load_pipeline_bench config=%s hw_concurrency=%u "
      "warmup=%d timed_runs=%d decode=%s (median wall_ms)\n",
      config_label, hw, kWarmup, kTimedRuns,
      g_geom_only ? "geom-only (no field UTF-8)"
                  : "load_ogr_feature_parts (geom+fields UTF-8)");
  std::printf("GDAL open excluded (one Open per cell)\n\n");

  const std::vector<Scheme> schemes = make_schemes();

  struct Row {
    const char* label = nullptr;
    std::string path;
    GIntBig ogr_count = -1;
    size_t expect_features = 0;
    std::vector<CellResult> cells;
  };
  std::vector<Row> rows;
  rows.reserve(std::size(kDatasets));
  int integrity_fails = 0;

  for (const DatasetSpec& ds : kDatasets) {
    Row row;
    row.label = ds.label;
    row.path = resolve_dataset(ds.relative);
    if (row.path.empty()) {
      std::fprintf(stderr, "SKIP missing dataset %s (%s)\n", ds.label,
                   ds.relative);
      continue;
    }

    {
      GDALDataset* probe =
          GDALDataset::Open(row.path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY);
      if (probe) {
        GIntBig total = 0;
        bool any = false;
        for (int li = 0; li < probe->GetLayerCount(); ++li) {
          OGRLayer* layer = probe->GetLayer(li);
          if (!layer || skip_text_layer(layer->GetName())) {
            continue;
          }
          const GIntBig n = layer->GetFeatureCount(/*bForce=*/TRUE);
          if (n >= 0) {
            total += n;
            any = true;
          }
        }
        if (any) {
          row.ogr_count = total;
        }
        GDALClose(probe);
      }
    }

    row.expect_features = serial_reference_count(row.path);
    std::printf(
        "dataset %-18s path=%s ogr_features=%lld expect_loaded=%zu\n",
        row.label, row.path.c_str(), static_cast<long long>(row.ogr_count),
        row.expect_features);

    if (stress_rounds > 0) {
      if (!stress_integrity(row.path, row.label, row.expect_features,
                            stress_rounds)) {
        ++integrity_fails;
      } else {
        std::printf("  stress w8 × %d rounds OK\n", stress_rounds);
      }
    }

    row.cells.reserve(schemes.size());
    for (const Scheme& scheme : schemes) {
      CellResult cell = bench_cell(row.path, scheme, row.expect_features);
      if (!cell.ok) {
        std::fprintf(stderr, "FAIL cell %s × %s\n", row.label, scheme.name);
        ++integrity_fails;
      } else if (!cell.count_ok) {
        ++integrity_fails;
      }
      row.cells.push_back(cell);
    }
    rows.push_back(std::move(row));
  }

  if (rows.empty()) {
    std::fprintf(stderr, "no datasets found; set SMARTGIS_ROOT or cwd\n");
    return 1;
  }

  std::puts("\n# CSV");
  std::fputs(
      "dataset,ogr_features,expect,scheme,wall_ms,features,parts,field_"
      "values,feat_per_s,count_ok\n",
      stdout);
  for (const Row& row : rows) {
    for (size_t si = 0; si < schemes.size(); ++si) {
      const CellResult& c = row.cells[si];
      std::printf("%s,%lld,%zu,%s,%.3f,%zu,%zu,%zu,%.1f,%s\n", row.label,
                  static_cast<long long>(row.ogr_count), row.expect_features,
                  schemes[si].name, c.wall_ms, c.features, c.parts,
                  c.field_values, c.feat_per_s, c.count_ok ? "ok" : "BAD");
    }
  }

  std::puts("\n# Markdown matrix (wall_ms / feat_per_s)");
  std::fputs("| dataset | features |", stdout);
  for (const Scheme& s : schemes) {
    std::printf(" %s |", s.name);
  }
  std::fputs("\n|---|---:|", stdout);
  for (size_t i = 0; i < schemes.size(); ++i) {
    std::fputs("---:|", stdout);
  }
  std::fputc('\n', stdout);

  for (const Row& row : rows) {
    std::printf("| %s | %zu |", row.label, row.expect_features);
    for (const CellResult& c : row.cells) {
      if (!c.ok) {
        std::fputs(" FAIL |", stdout);
      } else if (!c.count_ok) {
        std::printf(" BAD(%zu) |", c.features);
      } else {
        std::printf(" %.1f / %.0f |", c.wall_ms, c.feat_per_s);
      }
    }
    std::fputc('\n', stdout);
  }

  std::puts("\n# Scheme legend");
  std::puts(
      "- serial: force load_ogr_layer_serial (bypasses GetFeatureCount gate)");
  std::puts(
      "- pipe_wN_winW: serial_threshold=0, decode_workers=N, "
      "ordered_window=W");
  std::puts("- pipe_w4_win0: ordered_window=0 legacy buffer-all then sink");
  std::puts(
      "- product_defaults: FeatureLoadOptions{} (serial_threshold=64, "
      "workers=min(hw,8), ordered_window=256)");
  std::puts(
      "- count_ok: each timed run must load expect_loaded (== serial "
      "reference)");

  if (integrity_fails != 0) {
    std::fprintf(stderr, "feature_load_pipeline_bench: %d integrity fail(s)\n",
                 integrity_fails);
    return 1;
  }
  return 0;
}
