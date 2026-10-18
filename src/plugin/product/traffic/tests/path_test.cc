// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/gate.h"

#include "gtest/gtest.h"

TEST(TrafficProductGate, DepthChainPasses) {
  const plugin::ProductGateResult result = plugin::run_traffic_product_gate();
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
}
