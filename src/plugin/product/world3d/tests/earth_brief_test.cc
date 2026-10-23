// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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
                const std::filesystem::path& spec,
                const std::filesystem::path& report, std::string* text) {
  // _popen goes through cmd.exe, which strips a leading quote. These paths
  // have no spaces, so leave them unquoted.
  const std::string cmd = python + " " + script.string() + " " + pack.string() +
                          " " + spec.string() + " " + report.string() + " 2>&1";
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

const char* kEarthReport =
    "{\"ok\":true,\"op\":\"world3d.earth_system\",\"mode\":\"earth_system\","
    "\"space\":\"high_orbit\",\"sky\":true,\"sat_cloud\":true,"
    "\"land\":\"globe_dem\",\"sea\":\"globe_terrain\","
    "\"planar_ocean\":false,\"planar_cloud\":false,\"fog\":false,"
    "\"china_stack\":false,\"contour\":false,\"hex\":false,\"ortho\":false,"
    "\"wind\":false}";

const char* kMixedReport =
    "{\"ok\":true,\"op\":\"world3d.earth_system\",\"mode\":\"earth_system\","
    "\"space\":\"high_orbit\",\"land\":\"globe_dem\",\"sea\":\"globe_terrain\","
    "\"planar_ocean\":true}";

void write_driver(const std::filesystem::path& path) {
  std::ofstream out(path);
  out <<
      "import json, sys\n"
      "sys.path.insert(0, sys.argv[1])\n"
      "from ext import earth_brief\n"
      "class Host:\n"
      "    def __init__(self, report):\n"
      "        self.ids = []\n"
      "        self.payloads = []\n"
      "        self.report = report\n"
      "    def run_processing(self, pid, payload):\n"
      "        self.ids.append(pid)\n"
      "        self.payloads.append(payload)\n"
      "        return pid == 'world3d.earth_system'\n"
      "    def operation_result(self):\n"
      "        return self.report\n"
      "report = open(sys.argv[3], encoding='utf-8').read()\n"
      "host = Host(report)\n"
      "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
      "result = earth_brief(host, spec)\n"
      "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
      " 'payloads': host.payloads, 'error': result.get('error'),"
      " 'table': result.get('table'), 'csv': result.get('csv'),"
      " 'space': result.get('space'), 'land': result.get('land'),"
      " 'sea': result.get('sea'),"
      " 'planar_ocean': result.get('planar_ocean')}))\n";
}

}  // namespace

TEST(World3dEarthBrief, RecordsEarthSystemOnce) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_world3d_earth";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto out_dir = root / "brief";
  const std::filesystem::path pack(WORLD3D_EARTH_DIR);
  const auto spec_path = root / "spec.json";
  const auto report_path = root / "report.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"out_dir\":\"" << generic(out_dir) << "\"}";
  }
  {
    std::ofstream report_out(report_path);
    report_out << kEarthReport;
  }
  write_driver(driver);

  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, report_path, &text))
      << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 1u) << text;
  EXPECT_STREQ(ran["ids"][0].GetString(), "world3d.earth_system");
  EXPECT_STREQ(ran["payloads"][0].GetString(), "{}");
  EXPECT_STREQ(ran["space"].GetString(), "high_orbit");
  EXPECT_STREQ(ran["land"].GetString(), "globe_dem");
  EXPECT_STREQ(ran["sea"].GetString(), "globe_terrain");
  EXPECT_FALSE(ran["planar_ocean"].GetBool());

  const auto json_path = out_dir / "earth_brief.json";
  const auto csv_path = out_dir / "earth_brief.csv";
  ASSERT_TRUE(std::filesystem::is_regular_file(json_path));
  ASSERT_TRUE(std::filesystem::is_regular_file(csv_path));
  std::ifstream table_in(json_path);
  std::stringstream table_buf;
  table_buf << table_in.rdbuf();
  rapidjson::Document table;
  table.Parse(table_buf.str().c_str());
  ASSERT_FALSE(table.HasParseError());
  EXPECT_STREQ(table["space"].GetString(), "high_orbit");
  EXPECT_STREQ(table["land"].GetString(), "globe_dem");
  EXPECT_STREQ(table["sea"].GetString(), "globe_terrain");
  EXPECT_FALSE(table["planar_ocean"].GetBool());
  EXPECT_FALSE(table["planar_cloud"].GetBool());

  std::ifstream csv_in(csv_path);
  std::string csv((std::istreambuf_iterator<char>(csv_in)),
                  std::istreambuf_iterator<char>());
  EXPECT_NE(csv.find("space,high_orbit"), std::string::npos);
  EXPECT_NE(csv.find("land,globe_dem"), std::string::npos);
  EXPECT_NE(csv.find("sea,globe_terrain"), std::string::npos);
  EXPECT_NE(csv.find("planar_ocean,0"), std::string::npos);

  std::filesystem::remove_all(root, ec);
}

TEST(World3dEarthBrief, RejectsPlanarOceanWithGlobe) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_world3d_earth_reject";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const std::filesystem::path pack(WORLD3D_EARTH_DIR);
  const auto driver = root / "driver.py";
  write_driver(driver);

  const auto ask_spec = root / "ask.json";
  const auto ask_report = root / "ask_report.json";
  {
    std::ofstream spec_out(ask_spec);
    spec_out << "{\"out_dir\":\"" << generic(root / "unused")
             << "\",\"planar_ocean\":true}";
  }
  {
    std::ofstream report_out(ask_report);
    report_out << kEarthReport;
  }
  std::string asked;
  ASSERT_TRUE(run_python(python, driver, pack, ask_spec, ask_report, &asked))
      << asked;
  rapidjson::Document ask;
  ask.Parse(asked.c_str());
  ASSERT_FALSE(ask.HasParseError()) << asked;
  EXPECT_FALSE(ask["ok"].GetBool()) << asked;
  EXPECT_EQ(ask["ids"].Size(), 0u) << asked;
  EXPECT_FALSE(std::filesystem::exists(root / "unused" / "earth_brief.json"));

  const auto bad_spec = root / "bad.json";
  const auto bad_report = root / "bad_report.json";
  {
    std::ofstream spec_out(bad_spec);
    spec_out << "{\"out_dir\":\"" << generic(root / "bad_out") << "\"}";
  }
  {
    std::ofstream report_out(bad_report);
    report_out << kMixedReport;
  }
  std::string mixed;
  ASSERT_TRUE(run_python(python, driver, pack, bad_spec, bad_report, &mixed))
      << mixed;
  rapidjson::Document bad;
  bad.Parse(mixed.c_str());
  ASSERT_FALSE(bad.HasParseError()) << mixed;
  EXPECT_FALSE(bad["ok"].GetBool()) << mixed;
  ASSERT_EQ(bad["ids"].Size(), 1u) << mixed;
  EXPECT_STREQ(bad["ids"][0].GetString(), "world3d.earth_system");
  EXPECT_FALSE(std::filesystem::exists(root / "bad_out" / "earth_brief.json"));

  std::filesystem::remove_all(root, ec);
}
