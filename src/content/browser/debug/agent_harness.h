// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_HARNESS_H_
#define CONTENT_BROWSER_DEBUG_AGENT_HARNESS_H_

#include <string>

namespace content {
namespace detail {

// Spawn curated GIS / RHI unit-test and benchmark exes beside this process.
std::string run_gis_harness(bool benches);
std::string run_rhi_harness(bool benches);

// Handles :gis / :rhi console commands. Returns true when |line| is owned here.
bool exec_harness_command(const std::string& line, std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_HARNESS_H_
