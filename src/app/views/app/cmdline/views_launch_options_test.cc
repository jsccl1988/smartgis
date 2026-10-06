// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/cmdline/views_launch_options.h"

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
    auto o = parse_vec({L"SmartGIS.exe"});
    expect(o.ok, "default ok");
    expect(o.process_type == content::ProcessType::kBrowser, "default browser");
    expect(o.scenario_id.empty(), "default product (no scenario)");
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
    auto o = parse_vec({L"SmartGIS.exe", L"--self-test"});
    expect(o.ok && o.scenario_id == "self_test", "self-test");
    expect(app::is_harness_launch(o), "self-test is harness");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--self-test-console"});
    expect(o.ok && o.scenario_id == "console", "self-test-console");
  }
  {
    auto o = parse_vec(
        {L"SmartGIS.exe", L"--self-test", L"--self-test-console"});
    expect(o.ok && o.scenario_id == "console",
           "console wins over self-test");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--input-showcase"});
    expect(o.ok && o.scenario_id == "input", "input-showcase");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--atmosphere-showcase", L"legacy"});
    expect(o.ok && o.scenario_id == "atmosphere.legacy", "showcase legacy");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--atmosphere-showcase", L"ocean"});
    expect(o.ok && o.scenario_id == "atmosphere.ocean", "showcase ocean");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--map2d-showcase", L"china"});
    expect(o.ok && o.scenario_id == "map2d.china", "map2d showcase china");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--map2d-showcase=china"});
    expect(o.ok && o.scenario_id == "map2d.china", "map2d showcase=china");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--map2d-showcase=align"});
    expect(o.ok && o.scenario_id == "map2d.align", "map2d showcase=align");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--map2d-showcase=orthogrid"});
    expect(o.ok && o.scenario_id == "map2d.orthogrid",
           "map2d showcase=orthogrid");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--plugin-showcase=world3d"});
    expect(o.ok && o.scenario_id == "plugin.world3d",
           "plugin showcase=world3d");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase", L"print"});
    expect(o.ok && o.scenario_id == "plugin.print", "plugin showcase print");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--plugin-showcase=orthogrid"});
    expect(o.ok && o.scenario_id == "plugin.orthogrid",
           "plugin showcase=orthogrid");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--plugin-showcase=orthogrid3d"});
    expect(o.ok && o.scenario_id == "plugin.orthogrid3d",
           "plugin showcase=orthogrid3d");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--plugin-showcase=traffic"});
    expect(o.ok && o.scenario_id == "plugin.traffic",
           "plugin showcase=traffic");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase", L"flood"});
    expect(o.ok && o.scenario_id == "plugin.flood", "plugin showcase flood");
  }
  {
    auto o =
        parse_vec({L"SmartGIS.exe", L"--plugin-showcase=stormsurge"});
    expect(o.ok && o.scenario_id == "plugin.stormsurge",
           "plugin showcase=stormsurge");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase=mine"});
    expect(o.ok && o.scenario_id == "plugin.mine", "plugin showcase=mine");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase=geochem"});
    expect(o.ok && o.scenario_id == "plugin.geochem", "plugin showcase=geochem");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase=report"});
    expect(o.ok && o.scenario_id == "plugin.report", "plugin showcase=report");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--plugin-showcase=dem"});
    expect(o.ok && o.scenario_id == "plugin.world3d",
           "plugin showcase dem alias");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--ui-showcase", L"shell"});
    expect(o.ok && o.scenario_id == "ui.shell", "ui-showcase shell");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--ui-showcase=data"});
    expect(o.ok && o.scenario_id == "ui.data", "ui-showcase data");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--ui-showcase=interact"});
    expect(o.ok && o.scenario_id == "ui.interact", "ui-showcase interact");
  }
  {
    auto o = parse_vec(
        {L"SmartGIS.exe", L"--atmosphere-fields=a.nc:u,b.nc:v"});
    expect(o.ok && o.atmosphere_fields == "a.nc:u,b.nc:v", "fields");
    expect(o.scenario_id.empty(), "fields alone is product");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--shell-canvas=skia"});
    expect(o.ok && o.shell_canvas == "skia", "shell-canvas=skia");
  }
  {
    auto o = parse_vec({L"SmartGIS.exe", L"--atmosphere-showcase=ocean",
                       L"--self-test"});
    expect(o.ok && o.scenario_id == "atmosphere.ocean",
           "atmosphere ranks above self-test");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "views_launch_options_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_launch_options_test: ok\n");
  return 0;
}
