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

struct Host {
  int browser_main(const content::ContentMainParams&) const { return 10; }
  int renderer_main(const content::ContentMainParams&) const { return 20; }
  int gpu_main(const content::ContentMainParams&) const { return 30; }
  int utility_main(const content::ContentMainParams&) const { return 40; }
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

  wchar_t renderer[] = L"--type=renderer";
  wchar_t* renderer_argv[] = {exe, renderer};
  expect(content::content_main(params_with(renderer_argv, 2), host) == 20,
         "--type=renderer");

  wchar_t gpu[] = L"--type=gpu";
  wchar_t* gpu_argv[] = {exe, gpu};
  expect(content::content_main(params_with(gpu_argv, 2), host) == 30,
         "--type=gpu");

  wchar_t utility[] = L"--type=utility";
  wchar_t* utility_argv[] = {exe, utility};
  expect(content::content_main(params_with(utility_argv, 2), host) == 40,
         "--type=utility");

  {
    content::ContentMainParams params = params_with(browser_argv, 1);
    params.process_type = content::ProcessType::kGpu;
    params.process_type_set = true;
    expect(content::content_main(params, host) == 30,
           "process_type_set overrides argv");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "content_main_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("content_main_test: ok\n");
  return 0;
}
