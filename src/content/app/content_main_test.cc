// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/content_main.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

struct Host : content::ContentClient {
  int browser_calls = 0;

  int browser_main(const content::ContentMainParams&) override {
    ++browser_calls;
    return 10;
  }
};

content::ContentMainParams params_with(wchar_t** argv, int argc) {
  content::ContentMainParams params;
  params.argc = argc;
  params.argv = argv;
  return params;
}

}  // namespace

int main() {
  Host host;
  wchar_t exe[] = L"smartgis.exe";
  wchar_t* browser_argv[] = {exe};
  expect(content::content_main(params_with(browser_argv, 1), host) == 10,
         "missing --type= is browser");
  expect(host.browser_calls == 1, "browser calls the client once");

  // RendererMain has no invitation here, so it returns 3 without the client.
  wchar_t renderer[] = L"--type=renderer";
  wchar_t* renderer_argv[] = {exe, renderer};
  expect(content::content_main(params_with(renderer_argv, 2), host) == 3,
         "--type=renderer");
  expect(host.browser_calls == 1, "renderer does not call the client");

  // GpuMain rejects a launch with no parent pipe before creating a device.
  wchar_t gpu[] = L"--type=gpu";
  wchar_t* gpu_argv[] = {exe, gpu};
  expect(content::content_main(params_with(gpu_argv, 2), host) == 2,
         "--type=gpu");
  expect(host.browser_calls == 1, "gpu does not call the client");

  wchar_t utility[] = L"--type=utility";
  wchar_t* utility_argv[] = {exe, utility};
  expect(content::content_main(params_with(utility_argv, 2), host) == 1,
         "--type=utility");
  expect(host.browser_calls == 1, "utility does not call the client");

  {
    content::ContentMainParams params = params_with(browser_argv, 1);
    params.process_type = content::ProcessType::kGpu;
    params.process_type_set = true;
    expect(content::content_main(params, host) == 2,
           "process_type_set overrides argv");
    expect(host.browser_calls == 1, "override does not call the client");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "content_main_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("content_main_test: ok\n");
  return 0;
}
