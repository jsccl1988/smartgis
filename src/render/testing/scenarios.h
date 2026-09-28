// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_TESTING_SCENARIOS_H_
#define RENDER_TESTING_SCENARIOS_H_

#include <string>

#include "render/rhi/rhi.h"

namespace render {
namespace detail {

// Outcome of one industry-shaped RHI scenario (Null or GPU).
struct ScenarioResult {
  bool ok = false;
  // True when the backend cannot exercise the scenario (e.g. Null compute).
  bool skipped = false;
  std::string message;
};

ScenarioResult run_device_lifecycle(rhi::Backend backend);
ScenarioResult run_resources(rhi::Device* device);
ScenarioResult run_pipeline_bind(rhi::Device* device);
ScenarioResult run_record_present(rhi::Device* device);
ScenarioResult run_graph_present(rhi::Device* device);
ScenarioResult run_compute_smoke(rhi::Device* device);

// Runs the Null matrix. Returns first failure, or ok when all pass/skip.
ScenarioResult run_null_suite();

// Create + initialize |backend|. Caller owns the device (or nullptr on fail).
rhi::Device* create_initialized_device(rhi::Backend backend,
                                       const rhi::DeviceDesc& desc);

}  // namespace detail
}  // namespace render

#endif  // RENDER_TESTING_SCENARIOS_H_
