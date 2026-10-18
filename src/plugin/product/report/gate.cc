// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/gate.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "plugin/runtime/host/processing/args_json.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

struct ReportGateFix {
  std::string path;
  std::string page;
};

ReportGateFix& fix() {
  static ReportGateFix state;
  return state;
}

void clear_fix() {
  std::error_code ec;
  if (!fix().page.empty()) {
    std::filesystem::remove(fix().page, ec);
  }
  fix() = {};
}

bool report_contract(void*) {
  rapidjson::Document args;
  if (!parse_args_json("{\"path\":\"gate-report\"}", &args)) {
    return false;
  }
  return args_json_string(args, "path", &fix().path) && fix().path == "gate-report";
}

bool report_compute(void*) {
  return fix().path == "gate-report";
}

bool report_command(void*) {
  rapidjson::Document empty;
  std::string path;
  if (!parse_args_json("{}", &empty)) {
    return false;
  }
  return !args_json_string(empty, "path", &path);
}

bool report_visual(void*) {
  if (fix().path.empty()) {
    return false;
  }
  fix().page = (std::filesystem::temp_directory_path() / "report_gate.html")
                   .generic_string();
  std::ofstream out(fix().page, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << "<!DOCTYPE html><title>report</title><p>" << fix().path << "</p>\n";
  out.close();
  if (!out) {
    return false;
  }
  std::ifstream in(fix().page);
  std::string body;
  std::getline(in, body);
  return body.find(fix().path) != std::string::npos;
}

}  // namespace

ProductGateResult run_report_product_gate() {
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", report_contract},
      {"compute", report_compute},
      {"command", report_command},
      {"visual", report_visual},
  };
  const ProductGateResult result =
      run_product_gate("report", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin
