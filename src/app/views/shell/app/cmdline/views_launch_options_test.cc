// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

app::ViewsLaunchOptions parse_vec(std::vector<std::wstring> args) {
  std::vector<wchar_t*> argv;
  argv.reserve(args.size());
  for (auto& s : args) {
    argv.push_back(s.data());
  }
  return app::parse_views_launch_options(static_cast<int>(argv.size()),
                                         argv.data());
}

}  // namespace

int main() {
  {
    auto o = parse_vec({L"SmartGisViews.exe"});
    expect(o.ok, "default ok");
    expect(o.process_type == content::ProcessType::kBrowser, "default browser");
    expect(!o.self_test, "default no self-test");
    expect(o.atmosphere_showcase == app::AtmosphereShowcaseMode::kNone,
           "default no showcase");
    expect(o.shell_canvas.empty(), "default shell_canvas empty");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--type=gpu"});
    expect(o.ok && o.process_type == content::ProcessType::kGpu, "type=gpu");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--type", L"renderer"});
    expect(o.ok && o.process_type == content::ProcessType::kRenderer,
           "type renderer");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--self-test"});
    expect(o.ok && o.self_test, "self-test");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--atmosphere-showcase", L"ocean"});
    expect(o.ok && o.atmosphere_showcase == app::AtmosphereShowcaseMode::kOcean,
           "showcase ocean");
  }
  {
    auto o = parse_vec(
        {L"SmartGisViews.exe", L"--atmosphere-fields=a.nc:u,b.nc:v"});
    expect(o.ok && o.atmosphere_fields == "a.nc:u,b.nc:v", "fields");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--shell-canvas=skia"});
    expect(o.ok && o.shell_canvas == "skia", "shell-canvas=skia");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "views_launch_options_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_launch_options_test: ok\n");
  return 0;
}
