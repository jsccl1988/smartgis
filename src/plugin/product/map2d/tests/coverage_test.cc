// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/web_mercator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "rapidjson/document.h"

namespace {

std::string generic(const std::filesystem::path& path) {
  return path.generic_string();
}

std::string python312_exe() {
  const char* local = std::getenv("LOCALAPPDATA");
  if (local == nullptr || local[0] == '\0') {
    return {};
  }
  const auto path = std::filesystem::path(local) / "Programs" / "Python" /
                    "Python312" / "python.exe";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return {};
  }
  return path.string();
}

bool run_python(const std::string& python, const std::filesystem::path& script,
                const std::filesystem::path& pack,
                const std::filesystem::path& spec, std::string* text) {
  // _popen goes through cmd.exe, which strips a leading quote. These paths
  // have no spaces, so leave them unquoted.
  const std::string cmd = python + " " + script.string() + " " + pack.string() +
                          " " + spec.string() + " 2>&1";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return false;
  }
  std::string out;
  char buf[1024];
  while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
    out += buf;
  }
  const int rc = _pclose(pipe);
  if (text != nullptr) {
    *text = out;
  }
  return rc == 0;
}

double nice_step(double value) {
  if (!(value > 0.0)) {
    return 1.0;
  }
  const double exp = std::floor(std::log10(value));
  const double base = std::pow(10.0, exp);
  const double n = value / base;
  const double nice = (n < 1.5) ? 1.0 : (n < 3.5) ? 2.0 : (n < 7.5) ? 5.0 : 10.0;
  return nice * base;
}

// Same ground width and bar as plugin::layout_furniture, without linking the
// map viewport (that target pulls world3d static init).
struct BarOracle {
  double ground_width_m = 0;
  double scale_denominator = 0;
  double bar_meters = 0;
  int bar_len_px = 0;
};

BarOracle scale_bar(double west, double south, double east, double north,
                    int width_px, double dpi) {
  BarOracle bar;
  const double span = std::abs(east - west);
  const double lat = std::clamp((south + north) * 0.5,
                                -content::tile::k_max_mercator_lat,
                                content::tile::k_max_mercator_lat);
  constexpr double k_pi = 3.14159265358979323846;
  bar.ground_width_m = std::cos(lat * k_pi / 180.0) * 2.0 * k_pi *
                       content::tile::k_earth_radius_m * (span / 360.0);
  const double m_per_px = bar.ground_width_m / static_cast<double>(width_px);
  const double paper = 0.0254 / dpi;
  bar.scale_denominator = m_per_px / paper;
  bar.bar_meters = nice_step(m_per_px * 96.0);
  bar.bar_len_px =
      std::max(24, static_cast<int>(std::lround(bar.bar_meters / m_per_px)));
  const int inner_w = (width_px - 16) - 8;
  if (bar.bar_len_px > inner_w / 2) {
    bar.bar_len_px = std::max(24, inner_w / 3);
    bar.bar_meters = m_per_px * static_cast<double>(bar.bar_len_px);
  }
  return bar;
}

}  // namespace

TEST(Map2dCoverage, WritesTileTableAndOpensSelectedOnce) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  constexpr double k_west = 116.0;
  constexpr double k_south = 39.5;
  constexpr double k_east = 117.0;
  constexpr double k_north = 40.5;
  constexpr int k_selected = 8;
  constexpr int k_width = 1280;

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_map2d_coverage";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto out_dir = root / "table";
  std::filesystem::create_directories(out_dir, ec);

  content::tile::LonLatBox box;
  box.west = k_west;
  box.south = k_south;
  box.east = k_east;
  box.north = k_north;
  const auto tiles =
      content::tile::tiles_for_extent(box, k_selected, k_width, 1 << 20);
  ASSERT_FALSE(tiles.empty());
  const BarOracle bar =
      scale_bar(k_west, k_south, k_east, k_north, k_width, 96.0);
  EXPECT_GT(bar.ground_width_m, 0.0);
  EXPECT_GT(bar.scale_denominator, 1.0);

  const std::filesystem::path pack(MAP2D_COVERAGE_DIR);
  const auto spec_path = root / "spec.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"template\":\"https://tiles.example/{z}/{x}/{y}.png\","
             << "\"west\":116,\"south\":39.5,\"east\":117,\"north\":40.5,"
             << "\"zooms\":[6,8],\"selected\":8,\"out_dir\":\""
             << generic(out_dir) << "\"}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import coverage\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "        self.payloads = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        self.payloads.append(payload)\n"
        "        return pid == 'map2d.open_pyramid'\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = coverage(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'payloads': host.payloads, 'error': result.get('error'),"
        " 'table': result.get('table'), 'csv': result.get('csv'),"
        " 'tiles': result.get('selected_tile_count'),"
        " 'ground': result.get('ground_width_m'),"
        " 'den': result.get('scale_denominator'),"
        " 'bar_m': result.get('bar_meters'),"
        " 'bar_px': result.get('bar_len_px')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 1u) << text;
  EXPECT_STREQ(ran["ids"][0].GetString(), "map2d.open_pyramid");
  EXPECT_EQ(ran["tiles"].GetInt(), static_cast<int>(tiles.size()));
  EXPECT_NEAR(ran["ground"].GetDouble(), bar.ground_width_m, 1e-4);
  EXPECT_NEAR(ran["den"].GetDouble(), bar.scale_denominator, 1e-3);
  EXPECT_NEAR(ran["bar_m"].GetDouble(), bar.bar_meters, 1e-4);
  EXPECT_EQ(ran["bar_px"].GetInt(), bar.bar_len_px);

  rapidjson::Document present;
  present.Parse(ran["payloads"][0].GetString());
  ASSERT_FALSE(present.HasParseError());
  EXPECT_EQ(present["z"].GetInt(), k_selected);
  EXPECT_NEAR(present["west"].GetDouble(), k_west, 1e-9);
  EXPECT_NEAR(present["east"].GetDouble(), k_east, 1e-9);

  const auto json_path = out_dir / "coverage.json";
  const auto csv_path = out_dir / "coverage.csv";
  ASSERT_TRUE(std::filesystem::is_regular_file(json_path));
  ASSERT_TRUE(std::filesystem::is_regular_file(csv_path));
  std::ifstream table_in(json_path);
  std::stringstream table_buf;
  table_buf << table_in.rdbuf();
  rapidjson::Document table;
  table.Parse(table_buf.str().c_str());
  ASSERT_FALSE(table.HasParseError());
  ASSERT_EQ(table["rows"].Size(), 2u);
  EXPECT_EQ(table["rows"][0]["z"].GetInt(), 6);
  EXPECT_FALSE(table["rows"][0]["selected"].GetBool());
  EXPECT_EQ(table["rows"][1]["z"].GetInt(), 8);
  EXPECT_TRUE(table["rows"][1]["selected"].GetBool());
  EXPECT_EQ(table["rows"][1]["tile_count"].GetInt(),
            static_cast<int>(tiles.size()));

  std::filesystem::remove_all(root, ec);
}
