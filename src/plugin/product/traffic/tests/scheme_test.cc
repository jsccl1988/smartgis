// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/commands.h"

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "third_party/gn/dll_search.h"
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

double read_cost(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  rapidjson::Document doc;
  doc.Parse(buffer.str().c_str());
  if (doc.HasParseError() || !doc.HasMember("features") ||
      !doc["features"].IsArray() || doc["features"].Empty()) {
    return -1;
  }
  const auto& props = doc["features"][0]["properties"];
  if (!props.HasMember("cost")) {
    return -1;
  }
  return props["cost"].GetDouble();
}

}  // namespace

TEST(TrafficSchemeTable, WritesCostTableAndPresentsSelectedOnce) {
  prepare_dll_search_path();
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_traffic_schemes";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto network = root / "net.geojson";
  const auto out_dir = root / "table";
  std::filesystem::create_directories(out_dir, ec);
  {
    std::ofstream out(network);
    out << "{\n"
           "  \"type\": \"FeatureCollection\",\n"
           "  \"features\": [\n"
           "    {\"type\":\"Feature\",\"properties\":{\"delay\":5},\n"
           "     \"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,0]]}},\n"
           "    {\"type\":\"Feature\",\"properties\":{\"delay\":1},\n"
           "     \"geometry\":{\"type\":\"LineString\",\"coordinates\":[[1,0],[1,1]]}}\n"
           "  ]\n"
           "}\n";
  }

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(plugin::register_traffic(host));
  std::vector<std::string> processing_ids;
  host->set_processing_enqueue(
      [&](std::string processing_id, std::string args_json,
          content::ProcessingFactory factory) {
        processing_ids.push_back(std::move(processing_id));
        return factory(nullptr, args_json);
      });

  const std::string length_args =
      std::string("{\"network\":\"") + generic(network) + "\",\"output\":\"" +
      generic(out_dir / "path_00.geojson") +
      "\",\"start_x\":0,\"start_y\":0,\"end_x\":1,\"end_y\":1,\"frames\":1}";
  const std::string delay_args =
      std::string("{\"network\":\"") + generic(network) + "\",\"output\":\"" +
      generic(out_dir / "path_01.geojson") +
      "\",\"start_x\":0,\"start_y\":0,\"end_x\":1,\"end_y\":1,"
      "\"weight_field\":\"delay\",\"frames\":1}";
  EXPECT_TRUE(host->run_processing("traffic.cost_quote", length_args));
  EXPECT_TRUE(host->run_processing("traffic.cost_quote", delay_args));
  ASSERT_EQ(processing_ids.size(), 2u);
  EXPECT_EQ(processing_ids[0], "traffic.cost_quote");
  EXPECT_NEAR(read_cost(out_dir / "path_00.geojson"), 2.0, 1e-6);
  EXPECT_NEAR(read_cost(out_dir / "path_01.geojson"), 6.0, 1e-6);

  const std::filesystem::path pack(TRAFFIC_SCHEMES_DIR);
  const auto spec_path = root / "spec.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"network\":\"" << generic(network) << "\",\"out_dir\":\""
             << generic(out_dir)
             << "\",\"start_x\":0,\"start_y\":0,\"end_x\":1,\"end_y\":1,"
                "\"schemes\":[{\"id\":\"length\",\"weight_field\":\"\"},"
                "{\"id\":\"delay\",\"weight_field\":\"delay\"}],"
                "\"selected\":\"delay\"}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import scheme_table\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        return True\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = scheme_table(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'error': result.get('error'), 'cost': result.get('selected_cost')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 3u);
  EXPECT_STREQ(ran["ids"][0].GetString(), "traffic.cost_quote");
  EXPECT_STREQ(ran["ids"][1].GetString(), "traffic.cost_quote");
  EXPECT_STREQ(ran["ids"][2].GetString(), "traffic.cost_path");
  EXPECT_NEAR(ran["cost"].GetDouble(), 6.0, 1e-6);
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "schemes.json"));
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "schemes.csv"));

  delete host;
  std::filesystem::remove_all(root, ec);
}
