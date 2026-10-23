// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/commands.h"

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
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

double read_volume(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  rapidjson::Document doc;
  doc.Parse(buffer.str().c_str());
  if (doc.HasParseError() || !doc.HasMember("volume")) {
    return -1;
  }
  return doc["volume"].GetDouble();
}

}  // namespace

TEST(MineReserveTable, WritesVolumeTableAndPresentsSelectedTop) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_mine_reserves";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto csv = root / "holes.csv";
  const auto out_dir = root / "table";
  std::filesystem::create_directories(out_dir, ec);
  {
    std::ofstream out(csv);
    out << "hole_id,x,y,z,stratum_id\n"
           "BH1,0,0,110,cover_top\n"
           "BH1,0,0,105,cover_bottom\n"
           "BH2,10,0,112,cover_top\n"
           "BH2,10,0,106,cover_bottom\n"
           "BH3,10,10,111,cover_top\n"
           "BH3,10,10,105,cover_bottom\n"
           "BH4,0,10,109,cover_top\n"
           "BH4,0,10,104,cover_bottom\n"
           "BH1,0,0,100,ore_top\n"
           "BH1,0,0,90,ore_bottom\n"
           "BH2,10,0,102,ore_top\n"
           "BH2,10,0,91,ore_bottom\n"
           "BH3,10,10,101,ore_top\n"
           "BH3,10,10,89,ore_bottom\n"
           "BH4,0,10,99,ore_top\n"
           "BH4,0,10,88,ore_bottom\n";
  }

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(plugin::register_mine(host));
  std::vector<std::string> processing_ids;
  host->set_processing_enqueue(
      [&](std::string processing_id, std::string args_json,
          content::ProcessingFactory factory) {
        processing_ids.push_back(std::move(processing_id));
        return factory(nullptr, args_json);
      });

  const std::string cover =
      std::string("{\"input\":\"") + generic(csv) +
      "\",\"top_stratum_id\":\"cover_top\",\"bottom_stratum_id\":\"cover_bottom\","
      "\"output\":\"" +
      generic(out_dir / "volume_00.json") + "\"}";
  const std::string ore =
      std::string("{\"input\":\"") + generic(csv) +
      "\",\"top_stratum_id\":\"ore_top\",\"bottom_stratum_id\":\"ore_bottom\","
      "\"output\":\"" +
      generic(out_dir / "volume_01.json") + "\"}";
  EXPECT_TRUE(host->run_processing("mine.prism_volume", cover));
  EXPECT_TRUE(host->run_processing("mine.prism_volume", ore));
  ASSERT_EQ(processing_ids.size(), 2u);
  EXPECT_NEAR(read_volume(out_dir / "volume_00.json"), 550.0, 1e-6);
  EXPECT_NEAR(read_volume(out_dir / "volume_01.json"), 1100.0, 1e-6);

  const std::filesystem::path pack(MINE_RESERVES_DIR);
  const auto spec_path = root / "spec.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"input\":\"" << generic(csv) << "\",\"out_dir\":\""
             << generic(out_dir)
             << "\",\"pairs\":["
                "{\"id\":\"cover\",\"top_stratum_id\":\"cover_top\","
                "\"bottom_stratum_id\":\"cover_bottom\"},"
                "{\"id\":\"ore\",\"top_stratum_id\":\"ore_top\","
                "\"bottom_stratum_id\":\"ore_bottom\"}"
                "],\"selected\":\"ore\"}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import reserve_table\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        return True\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = reserve_table(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'error': result.get('error'), 'volume': result.get('selected_volume')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 3u);
  EXPECT_STREQ(ran["ids"][0].GetString(), "mine.prism_volume");
  EXPECT_STREQ(ran["ids"][1].GetString(), "mine.prism_volume");
  EXPECT_STREQ(ran["ids"][2].GetString(), "mine.interpolate_stratum");
  EXPECT_NEAR(ran["volume"].GetDouble(), 1100.0, 1e-6);
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "reserves.json"));
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "reserves.csv"));

  delete host;
  std::filesystem::remove_all(root, ec);
}
