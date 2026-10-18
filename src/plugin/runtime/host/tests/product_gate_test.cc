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

#include "gtest/gtest.h"

namespace {

bool g_later_ran = false;

bool fail_contract(void*) {
  return false;
}

bool later_step(void*) {
  g_later_ran = true;
  return true;
}

bool pass_step(void*) {
  return true;
}

std::string read_report(const char* package) {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  EXPECT_GT(n, 0u);
  const std::filesystem::path path = std::filesystem::path(buf).parent_path() /
                                     "captures" / "plugin" / package /
                                     "gate.json";
  std::ifstream in(path);
  std::string text;
  std::getline(in, text);
  return text;
}

}  // namespace

TEST(ProductGate, StopsBeforeLaterStep) {
  g_later_ran = false;
  const plugin::ProductGateStep steps[] = {
      {"contract", fail_contract},
      {"compute", later_step},
  };
  const plugin::ProductGateResult result =
      plugin::run_product_gate("gateprobe", steps, 2, nullptr);
  EXPECT_FALSE(result.passed);
  ASSERT_NE(result.stopped_at, nullptr);
  EXPECT_STREQ(result.stopped_at, "contract");
  EXPECT_FALSE(g_later_ran);
  const std::string json = read_report("gateprobe");
  EXPECT_NE(json.find("\"baseline\":false"), std::string::npos);
  EXPECT_NE(json.find("\"stopped_at\":\"contract\""), std::string::npos);
}

TEST(ProductGate, PassWritesBaseline) {
  const plugin::ProductGateStep steps[] = {
      {"contract", pass_step},
      {"compute", pass_step},
  };
  const plugin::ProductGateResult result =
      plugin::run_product_gate("gatepass", steps, 2, nullptr);
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
  const std::string json = read_report("gatepass");
  EXPECT_NE(json.find("\"baseline\":true"), std::string::npos);
  EXPECT_NE(json.find("\"stopped_at\":null"), std::string::npos);
}
