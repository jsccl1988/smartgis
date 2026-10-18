// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/gate.h"

#include "gtest/gtest.h"

TEST(ReportProductGate, DepthChainPasses) {
  const plugin::ProductGateResult result = plugin::run_report_product_gate();
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
}
