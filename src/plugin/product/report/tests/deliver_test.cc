// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

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
  const std::string cmd = python + " " + script.string() + " " + pack.string() +
                          " " + spec.string() + " 2>&1";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return false;
  }
  std::string out;
  char buf[2048];
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

TEST(ReportDeliver, WritesHtmlTableAndOpensOnce) {
  const std::string python = python312_exe();
  if (python.empty()) {
    GTEST_SKIP() << "Python 3.12 is not installed";
  }

  const auto root =
      std::filesystem::temp_directory_path() / "smartgis_report_deliver";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  const auto out_dir = root / "pack";

  const std::filesystem::path pack(REPORT_DELIVER_DIR);
  const auto spec_path = root / "spec.json";
  const auto driver = root / "driver.py";
  {
    std::ofstream spec_out(spec_path);
    spec_out << "{\"title\":\"level curve\",\"out_dir\":\"" << generic(out_dir)
             << "\",\"columns\":[\"level\",\"area\"],\"rows\":["
                "{\"level\":2.5,\"area\":3},"
                "{\"level\":10,\"area\":64}"
                "],\"selected\":0}";
  }
  {
    std::ofstream driver_out(driver);
    driver_out <<
        "import json, sys\n"
        "sys.path.insert(0, sys.argv[1])\n"
        "from ext import materialize\n"
        "class Host:\n"
        "    def __init__(self):\n"
        "        self.ids = []\n"
        "    def run_processing(self, pid, payload):\n"
        "        self.ids.append(pid)\n"
        "        return True\n"
        "host = Host()\n"
        "spec = json.load(open(sys.argv[2], encoding='utf-8'))\n"
        "result = materialize(host, spec)\n"
        "print(json.dumps({'ok': result.get('ok'), 'ids': host.ids,"
        " 'error': result.get('error'), 'html': result.get('html')}))\n";
  }
  std::string text;
  ASSERT_TRUE(run_python(python, driver, pack, spec_path, &text)) << text;
  rapidjson::Document ran;
  ran.Parse(text.c_str());
  ASSERT_FALSE(ran.HasParseError()) << text;
  EXPECT_TRUE(ran["ok"].GetBool()) << text;
  ASSERT_EQ(ran["ids"].Size(), 1u);
  EXPECT_STREQ(ran["ids"][0].GetString(), "report.open");

  const auto html_path = out_dir / "index.html";
  const auto data_path = out_dir / "data.json";
  ASSERT_TRUE(std::filesystem::is_regular_file(html_path));
  ASSERT_TRUE(std::filesystem::is_regular_file(data_path));
  ASSERT_TRUE(std::filesystem::is_regular_file(out_dir / "table.csv"));
  std::ifstream html_in(html_path);
  std::stringstream html;
  html << html_in.rdbuf();
  const std::string page = html.str();
  EXPECT_NE(page.find("data-selected=\"1\""), std::string::npos);
  EXPECT_NE(page.find(">2.5<"), std::string::npos);
  EXPECT_NE(page.find(">64<"), std::string::npos);
  std::ifstream data_in(data_path);
  std::stringstream data;
  data << data_in.rdbuf();
  rapidjson::Document doc;
  doc.Parse(data.str().c_str());
  ASSERT_FALSE(doc.HasParseError());
  EXPECT_EQ(doc["selected"].GetInt(), 0);
  ASSERT_EQ(doc["rows"].Size(), 2u);
  std::filesystem::remove_all(root, ec);
}
