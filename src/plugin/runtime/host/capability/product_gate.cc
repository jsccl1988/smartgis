// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/product_gate.h"

#include <filesystem>
#include <fstream>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/core/log.h"

namespace plugin {
namespace {

bool package_name_ok(const char* package) {
  if (package == nullptr || package[0] == '\0') {
    return false;
  }
  for (const char* p = package; *p != '\0'; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!alnum) {
      return false;
    }
  }
  return true;
}

std::filesystem::path exe_directory() {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buf).parent_path();
}

}  // namespace

ProductGateResult eval_product_gate(const char* package,
                                    const ProductGateStep* steps, size_t count,
                                    void* ctx) {
  ProductGateResult result;
  if (!package_name_ok(package) || steps == nullptr || count == 0) {
    result.stopped_at = "package";
    LOGGING(LOG_ERROR, "product gate rejected package");
    return result;
  }
  for (size_t i = 0; i < count; ++i) {
    const ProductGateStep& step = steps[i];
    if (step.name == nullptr || step.name[0] == '\0' || step.run == nullptr ||
        !step.run(ctx)) {
      result.stopped_at = (step.name != nullptr && step.name[0] != '\0')
                              ? step.name
                              : "contract";
      LOGGING(LOG_ERROR, "product gate %s stopped at %s", package,
              result.stopped_at);
      return result;
    }
  }
  result.passed = true;
  LOGGING(LOG_INFO, "product gate %s passed", package);
  return result;
}

bool write_product_gate_report(const char* package,
                               const ProductGateResult& result) {
  if (!package_name_ok(package)) {
    LOGGING(LOG_ERROR, "product gate report rejected package");
    return false;
  }
  const std::filesystem::path dir =
      exe_directory() / "captures" / "plugin" / package;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    LOGGING(LOG_ERROR, "product gate report mkdir failed: %s",
            ec.message().c_str());
    return false;
  }
  const std::filesystem::path path = dir / "gate.json";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    LOGGING(LOG_ERROR, "product gate report open failed");
    return false;
  }
  out << "{\"package\":\"" << package << "\",\"passed\":"
      << (result.passed ? "true" : "false") << ",\"stopped_at\":";
  if (result.passed || result.stopped_at == nullptr) {
    out << "null";
  } else {
    out << "\"" << result.stopped_at << "\"";
  }
  out << ",\"baseline\":" << (result.passed ? "true" : "false") << "}\n";
  out.flush();
  if (!out) {
    LOGGING(LOG_ERROR, "product gate report write failed");
    return false;
  }
  return true;
}

}  // namespace plugin
