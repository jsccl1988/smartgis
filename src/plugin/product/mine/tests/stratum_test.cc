// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/gate.h"

#include "gtest/gtest.h"

TEST(MineProductGate, DepthChainPasses) {
  const plugin::ProductGateResult result = plugin::run_mine_product_gate();
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
}
