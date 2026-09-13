// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/renderer_main.h"

#include <cstdio>
#include <cwchar>

namespace content {
namespace {

bool argv_has_flag(int argc, wchar_t** argv, const wchar_t* flag) {
  if (!argv) {
    return false;
  }
  for (int i = 0; i < argc; ++i) {
    if (argv[i] && wcscmp(argv[i], flag) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

int RendererMain(const ContentMainParams& params) {
  // This TU must stay free of d3d11.h / GL. Paint lives in gpu::GpuMain.
  if (argv_has_flag(params.argc, params.argv, L"--self-test")) {
    std::fprintf(stdout, "--type=renderer --self-test: no GPU device\n");
    return 0;
  }
  // Named-pipe present server stays in GpuMain until MapWidget exists.
  return 0;
}

}  // namespace content
