// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/testing/scenarios.h"

#include <cstdio>

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  const render::detail::ScenarioResult r = render::detail::run_null_suite();
  if (!r.ok) {
    std::fprintf(stderr, "rhi_suite_test: FAIL %s\n", r.message.c_str());
    return 1;
  }
  std::fprintf(stdout, "rhi_suite_test: ok (%s)\n", r.message.c_str());
  return 0;
}
