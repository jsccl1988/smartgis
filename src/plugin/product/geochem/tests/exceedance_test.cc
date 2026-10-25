// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/commands.h"

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "gis/analysis/geochem/load/csv_load.h"
#include "gis/analysis/geochem/model/sample.h"
#include "gis/analysis/geochem/stats/stats.h"
#include "tool/command/command.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

int count_exceed(const std::string& csv, double k_sigma, double* threshold) {
  const gis::detail::GeochemSampleSet set = gis::detail::load_csv(csv);
  const gis::detail::GeochemElementStats stats =
      gis::detail::compute_stats(set, "Cu", 10, k_sigma);
  if (!stats.ok || threshold == nullptr) {
    return -1;
  }
  *threshold = stats.threshold;
  const int index = gis::detail::element_index(set, "Cu");
  int exceed = 0;
  for (const gis::detail::GeochemSample& sample : set.samples) {
    if (index >= 0 && index < static_cast<int>(sample.values.size()) &&
        sample.values[static_cast<size_t>(index)] >= stats.threshold) {
      ++exceed;
    }
  }
  return exceed;
}

bool run_python(const std::string& python, const std::filesystem::path& script,
                const std::filesystem::path& pack,
                const std::filesystem::path& spec, std::string* text) {
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

}  // namespace

TEST(GeochemExceedance, WritesThresholdTableAndPresentsSelectedOnce) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_geochem_grades";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto csv = root / "samples.csv";
  const auto out_dir = root / "table";
  std::filesystem::create_directories(out_dir, ec);
  {
    std::ofstream out(csv);
    out << "id,lon,lat,Cu,Au\n"
           "A,0,0,10,1\n"
           "B,1,0,12,1.1\n"
           "C,1,1,40,3.5\n"
           "D,0,1,38,3.2\n"
           "E,0.5,0.5,15,1.4\n"
           "F,0.8,0.2,42,3.8\n";
  }

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(plugin::register_geochem(host));
  std::vector<std::string> processing_ids;
  host->set_processing_enqueue(
      [&](std::string processing_id, std::string args_json,
          content::ProcessingFactory factory) {
        processing_ids.push_back(std::move(processing_id));
        return factory(nullptr, args_json);
      });

  const std::string low =
      std::string("{\"input\":\"") + generic(csv) +
      "\",\"element\":\"Cu\",\"k_sigma\":0.5,\"output\":\"" +
      generic(out_dir / "tally_00.json") + "\"}";
  const std::string high =
      std::string("{\"input\":\"") + generic(csv) +
      "\",\"element\":\"Cu\",\"k_sigma\":4,\"output\":\"" +
      generic(out_dir / "tally_01.json") + "\"}";
  EXPECT_TRUE(host->run_processing("geochem.threshold_tally", low));
  EXPECT_TRUE(host->run_processing("geochem.threshold_tally", high));
  ASSERT_EQ(processing_ids.size(), 2u);
  EXPECT_EQ(processing_ids[0], "geochem.threshold_tally");
  EXPECT_EQ(processing_ids[1], "geochem.threshold_tally");

  double low_threshold = 0;
  double high_threshold = 0;
  const int low_exceed = count_exceed(generic(csv), 0.5, &low_threshold);
  const int high_exceed = count_exceed(generic(csv), 4.0, &high_threshold);
  ASSERT_GE(low_exceed, 0);
  ASSERT_GE(high_exceed, 0);
  EXPECT_GT(low_exceed, high_exceed);
  EXPECT_LT(low_threshold, high_threshold);

  for (const char* name : {"tally_00.json", "tally_01.json"}) {
    std::ifstream in(out_dir / name);
    std::stringstream buffer;
    buffer << in.rdbuf();
    rapidjson::Document doc;
    doc.Parse(buffer.str().c_str());
    ASSERT_FALSE(doc.HasParseError()) << name;
    EXPECT_EQ(doc["count"].GetInt(), 6);
  }

  const std::filesystem::path pack(GEOCHEM_GRADES_DIR);
  const auto spec_path = root / "spec.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"input\":\"" << generic(csv) << "\",\"element\":\"Cu\","
             << "\"out_dir\":\"" << generic(out_dir)
             << "\",\"k_sigmas\":[0.5,4],\"selected\":0.5}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import exceedance\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        return True\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = exceedance(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'table': result.get('table'), 'error': result.get('error'),"
        " 'exceed': result.get('selected_exceed_count')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 3u);
  EXPECT_STREQ(ran["ids"][0].GetString(), "geochem.threshold_tally");
  EXPECT_STREQ(ran["ids"][1].GetString(), "geochem.threshold_tally");
  EXPECT_STREQ(ran["ids"][2].GetString(), "geochem.stats");
  EXPECT_EQ(ran["exceed"].GetInt(), low_exceed);
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "exceedance.json"));
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "exceedance.csv"));

  delete host;
  std::filesystem::remove_all(root, ec);
}
