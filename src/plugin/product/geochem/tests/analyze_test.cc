// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/gate.h"

#include "gtest/gtest.h"

TEST(GeochemProductGate, DepthChainPasses) {
  const plugin::ProductGateResult result = plugin::run_geochem_product_gate();
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
}
