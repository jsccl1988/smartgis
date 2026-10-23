// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/commands.h"

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "tool/command/command.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gtest/gtest.h"
#include "rapidjson/document.h"

namespace {

std::string generic(const std::filesystem::path& path) {
  return path.generic_string();
}

bool write_pocket_dem(const std::filesystem::path& path) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (driver == nullptr) {
    return false;
  }
  const std::string text = generic(path);
  VSIUnlink(text.c_str());
  constexpr int kW = 8;
  constexpr int kH = 8;
  GDALDatasetUniquePtr ds(
      driver->Create(text.c_str(), kW, kH, 1, GDT_Float32, nullptr));
  if (!ds) {
    return false;
  }
  double gt[6] = {0, 1, 0, 0, 0, -1};
  ds->SetGeoTransform(gt);
  std::vector<float> elev(static_cast<size_t>(kW * kH), 10.0f);
  elev[2 * kW + 2] = 1.0f;
  elev[2 * kW + 3] = 2.0f;
  elev[3 * kW + 2] = 2.0f;
  elev[3 * kW + 3] = 3.0f;
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (band == nullptr) {
    return false;
  }
  return band->RasterIO(GF_Write, 0, 0, kW, kH, elev.data(), kW, kH,
                        GDT_Float32, 0, 0, nullptr) == CE_None;
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
                const std::filesystem::path& extra, std::string* text) {
  const std::string cmd = python + " " + script.string() + " " + pack.string() +
                          " " + extra.string() + " 2>&1";
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

TEST(StormsurgeLevelCurve, WritesAreaTableAndPresentsSelectedOnce) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_stormsurge_levels";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto dem = root / "dem.tif";
  const auto out_dir = root / "curve";
  ASSERT_TRUE(write_pocket_dem(dem));
  std::filesystem::create_directories(out_dir, ec);

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(plugin::register_stormsurge(host));
  std::vector<std::string> processing_ids;
  host->set_processing_enqueue(
      [&](std::string processing_id, std::string args_json,
          content::ProcessingFactory factory) {
        processing_ids.push_back(std::move(processing_id));
        return factory(nullptr, args_json);
      });

  const auto low_mask = out_dir / "mask_00.tif";
  const auto high_mask = out_dir / "mask_01.tif";
  const std::string low_args =
      std::string("{\"dem\":\"") + generic(dem) + "\",\"output\":\"" +
      generic(low_mask) +
      "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":2.5,\"frames\":1}";
  const std::string high_args =
      std::string("{\"dem\":\"") + generic(dem) + "\",\"output\":\"" +
      generic(high_mask) +
      "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":10,\"frames\":1}";
  EXPECT_TRUE(host->run_processing("stormsurge.level_mask", low_args));
  EXPECT_TRUE(host->run_processing("stormsurge.level_mask", high_args));
  ASSERT_EQ(processing_ids.size(), 2u);
  EXPECT_EQ(processing_ids[0], "stormsurge.level_mask");

  const std::filesystem::path pack(STORMSURGE_LEVELS_DIR);
  const auto area_py = root / "area.py";
  {
    std::ofstream out(area_py);
    out << "import json, sys\n"
           "sys.path.insert(0, sys.argv[1])\n"
           "from ext import mask_area\n"
           "print(json.dumps(mask_area(sys.argv[2])))\n";
  }
  std::string low_text;
  std::string high_text;
  ASSERT_TRUE(run_python(python, area_py, pack, low_mask, &low_text))
      << low_text;
  ASSERT_TRUE(run_python(python, area_py, pack, high_mask, &high_text))
      << high_text;
  rapidjson::Document low_doc;
  rapidjson::Document high_doc;
  low_doc.Parse(low_text.c_str());
  high_doc.Parse(high_text.c_str());
  ASSERT_FALSE(low_doc.HasParseError()) << low_text;
  ASSERT_FALSE(high_doc.HasParseError()) << high_text;
  EXPECT_EQ(low_doc["flooded_cells"].GetInt(), 3);
  EXPECT_DOUBLE_EQ(low_doc["area_map"].GetDouble(), 3.0);
  EXPECT_EQ(high_doc["flooded_cells"].GetInt(), 64);
  EXPECT_DOUBLE_EQ(high_doc["area_map"].GetDouble(), 64.0);

  const auto spec_path = root / "spec.json";
  const auto driver = root / "curve_driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"dem\":\"" << generic(dem) << "\",\"out_dir\":\""
             << generic(out_dir)
             << "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"levels\":[2.5,10],"
                "\"selected\":2.5}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import level_curve\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        return True\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = level_curve(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'error': result.get('error'), 'area': result.get('selected_area')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 3u);
  EXPECT_STREQ(ran["ids"][0].GetString(), "stormsurge.level_mask");
  EXPECT_STREQ(ran["ids"][1].GetString(), "stormsurge.level_mask");
  EXPECT_STREQ(ran["ids"][2].GetString(), "stormsurge.run");
  EXPECT_DOUBLE_EQ(ran["area"].GetDouble(), 3.0);
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "curve.json"));
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "curve.csv"));

  delete host;
  std::filesystem::remove_all(root, ec);
}
