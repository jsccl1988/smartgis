// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/cmdline/views_launch_options.h"

#include <cstdio>
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
    auto o = parse_vec({L"SmartGIS.exe"});
    expect(o.ok, "default ok");
    expect(o.process_type == content::ProcessType::kBrowser, "default browser");
    expect(o.scenario_id.empty(), "CLI does not pick a scenario");
    expect(!app::is_harness_launch(o), "default not harness");
    expect(o.shell_canvas.empty(), "default shell_canvas empty");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--type=gpu"});
    expect(o.ok && o.process_type == content::ProcessType::kGpu, "type=gpu");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--type", L"renderer"});
    expect(o.ok && o.process_type == content::ProcessType::kRenderer,
           "type renderer");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase=flood"});
    expect(o.ok && o.scenario_id.empty(),
           "plugin-showcase extra ignored (plugin.json owns scenario)");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--harness"});
    expect(o.ok && o.scenario_id.empty(), "harness extra ignored");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--shell-canvas=skia"});
    expect(o.ok && o.shell_canvas == "skia", "shell-canvas=skia");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--debug-console"});
    expect(o.ok && o.debug_console, "debug-console");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugins-dir", L"D:\\plugins"});
    expect(o.ok && o.plugins_dir == "D:\\plugins", "plugins-dir");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "views_launch_options_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_launch_options_test: ok\n");
  return 0;
}
