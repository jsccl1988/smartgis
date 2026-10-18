// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/gate.h"

#include "plugin/runtime/host/capability/product_gate.h"

#include "gtest/gtest.h"

TEST(World3dProductGate, ContractAndComputePass) {
  const plugin::ProductGateStep steps[] = {
      {"contract", plugin::world3d_gate_contract},
      {"compute", plugin::world3d_gate_compute},
  };
  const plugin::ProductGateResult result =
      plugin::eval_product_gate("world3d", steps, 2, nullptr);
  EXPECT_TRUE(result.passed);
  EXPECT_EQ(result.stopped_at, nullptr);
}
