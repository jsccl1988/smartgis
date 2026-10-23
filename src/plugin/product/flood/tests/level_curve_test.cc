// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/ext_module.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/product/flood/commands.h"
#include "plugin/runtime/extension/runtime.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/message_box.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

// Debug embeds no CPython (python_enabled is Release-only). The area reader
// still runs under the installed Python 3.12 so the mask contract is tested.
bool python_mask_area(const std::string& python, const std::filesystem::path& pack,
                      const std::filesystem::path& mask,
                      const std::filesystem::path& script, int* cells,
                      double* area) {
  std::ofstream out(script);
  out << "import json, sys\n"
         "sys.path.insert(0, sys.argv[1])\n"
         "from ext import mask_area\n"
         "print(json.dumps(mask_area(sys.argv[2])))\n";
  out.close();
  // _popen goes through cmd.exe, which strips a leading quote. These paths
  // have no spaces, so leave them unquoted.
  const std::string cmd = python + " " + script.string() + " " + pack.string() +
                          " " + mask.string() + " 2>&1";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    std::fprintf(stderr, "popen failed: %s\n", cmd.c_str());
    return false;
  }
  std::string text;
  char buf[512];
  while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
    text += buf;
  }
  const int rc = _pclose(pipe);
  if (rc != 0) {
    std::fprintf(stderr, "mask_area rc=%d cmd=%s\n%s\n", rc, cmd.c_str(),
                 text.c_str());
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(text.c_str());
  if (doc.HasParseError() || !doc.HasMember("flooded_cells") ||
      !doc.HasMember("area_map")) {
    return false;
  }
  *cells = doc["flooded_cells"].GetInt();
  *area = doc["area_map"].GetDouble();
  return true;
}

class SuppressMessageBox {
 public:
  SuppressMessageBox() {
    ui::views::set_message_box_suppressed_for_test(true);
  }
  ~SuppressMessageBox() {
    ui::views::set_message_box_suppressed_for_test(false);
  }
};

}  // namespace

TEST(FloodLevelCurve, WritesAreaTableAndPresentsSelectedOnce) {
  const SuppressMessageBox suppress;
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_flood_levels";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto dem = root / "dem.tif";
  const auto out_dir = root / "curve";
  ASSERT_TRUE(write_pocket_dem(dem));

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  ASSERT_NE(host, nullptr);
  ASSERT_TRUE(plugin::register_flood(host));

  std::vector<std::string> processing_ids;
  host->set_processing_enqueue(
      [&](std::string processing_id, std::string args_json,
          content::ProcessingFactory factory) {
        processing_ids.push_back(std::move(processing_id));
        return factory(nullptr, args_json);
      });

  const std::filesystem::path pack(FLOOD_LEVELS_DIR);
  const auto low_mask = out_dir / "mask_00.tif";
  const auto high_mask = out_dir / "mask_01.tif";
  std::filesystem::create_directories(out_dir, ec);
  const std::string low_args =
      std::string("{\"dem\":\"") + generic(dem) + "\",\"output\":\"" +
      generic(low_mask) +
      "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":2.5,\"frames\":1}";
  const std::string high_args =
      std::string("{\"dem\":\"") + generic(dem) + "\",\"output\":\"" +
      generic(high_mask) +
      "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":10,\"frames\":1}";
  EXPECT_TRUE(host->run_processing("flood.level_mask", low_args));
  EXPECT_TRUE(host->run_processing("flood.level_mask", high_args));
  ASSERT_EQ(processing_ids.size(), 2u);
  EXPECT_EQ(processing_ids[0], "flood.level_mask");
  EXPECT_EQ(processing_ids[1], "flood.level_mask");

  int cells = 0;
  double area = 0;
  ASSERT_TRUE(python_mask_area(python, pack, low_mask, root / "area.py",
                               &cells, &area));
  EXPECT_EQ(cells, 3);
  EXPECT_DOUBLE_EQ(area, 3.0);
  ASSERT_TRUE(python_mask_area(python, pack, high_mask, root / "area.py",
                               &cells, &area));
  EXPECT_EQ(cells, 64);
  EXPECT_DOUBLE_EQ(area, 64.0);

  {
    const auto driver = root / "curve_driver.py";
    const auto spec_path = root / "spec.json";
    std::ofstream spec_out(spec_path);
    spec_out << "{\"dem\":\"" << generic(dem) << "\",\"out_dir\":\""
             << generic(out_dir)
             << "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"levels\":[2.5,10],"
                "\"selected\":2.5}";
    spec_out.close();
    std::ofstream driver_out(driver);
    driver_out << "import json, sys\n"
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
                   " 'table': result.get('table'), 'error': result.get('error')}))\n";
    driver_out.close();
    const std::string cmd = python + " " + driver.string() + " " +
                            pack.string() + " " + spec_path.string() + " 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    ASSERT_NE(pipe, nullptr);
    std::string text;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
      text += buf;
    }
    ASSERT_EQ(_pclose(pipe), 0) << text;
    rapidjson::Document ran;
    ran.Parse(text.c_str());
    ASSERT_FALSE(ran.HasParseError()) << text;
    EXPECT_TRUE(ran["ok"].GetBool()) << text;
    ASSERT_EQ(ran["ids"].Size(), 3u);
    EXPECT_STREQ(ran["ids"][0].GetString(), "flood.level_mask");
    EXPECT_STREQ(ran["ids"][1].GetString(), "flood.level_mask");
    EXPECT_STREQ(ran["ids"][2].GetString(), "flood.inundate");
    ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "curve.json"));
    ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "curve.csv"));
  }

  plugin::PythonRuntime py;
  if (py.init()) {
    processing_ids.clear();
    py.bind_host(host);
    ASSERT_TRUE(content::py::mount_extension("smartgis.flood_levels",
                                             generic(pack)));
    ASSERT_TRUE(py.start(generic(pack), "plugin.py", host));
    const std::string payload =
        std::string("{\"dem\":\"") + generic(dem) + "\",\"out_dir\":\"" +
        generic(out_dir) +
        "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"levels\":[2.5,10],"
        "\"selected\":2.5}";
    tool::CommandArgs args;
    args.payload = payload;
    EXPECT_TRUE(host->execute("flood.level_curve", args));
    ASSERT_EQ(processing_ids.size(), 3u);
    EXPECT_EQ(processing_ids[0], "flood.level_mask");
    EXPECT_EQ(processing_ids[1], "flood.level_mask");
    EXPECT_EQ(processing_ids[2], "flood.inundate");

    const auto table_path = out_dir / "curve.json";
    ASSERT_TRUE(std::filesystem::is_regular_file(table_path));
    ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "curve.csv"));
    std::ifstream in(table_path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    rapidjson::Document doc;
    doc.Parse(buffer.str().c_str());
    ASSERT_FALSE(doc.HasParseError());
    ASSERT_EQ(doc["rows"].Size(), 2u);
    EXPECT_EQ(doc["rows"][0]["flooded_cells"].GetInt(), 3);
    EXPECT_TRUE(doc["rows"][0]["selected"].GetBool());
    EXPECT_EQ(doc["rows"][1]["flooded_cells"].GetInt(), 64);
    EXPECT_FALSE(doc["rows"][1]["selected"].GetBool());
    py.stop(generic(pack));
    content::py::unmount_extension("smartgis.flood_levels");
    py.shutdown();
  }

  delete host;
  std::filesystem::remove_all(root, ec);
}
