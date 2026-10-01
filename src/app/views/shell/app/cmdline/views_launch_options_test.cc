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
    expect(o.map2d_showcase == app::Map2dShowcaseMode::kNone,
           "default no map2d showcase");
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
    expect(!o.self_test_console, "self-test alone no console");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--self-test-console"});
    expect(o.ok && o.self_test_console, "self-test-console");
    expect(!o.self_test, "console alone no self-test");
  }
  {
    auto o = parse_vec(
        {L"SmartGisViews.exe", L"--self-test", L"--self-test-console"});
    expect(o.ok && o.self_test && o.self_test_console, "both self-test flags");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--input-showcase"});
    expect(o.ok && o.input_showcase, "input-showcase");
    expect(!o.self_test, "input-showcase alone no self-test");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--atmosphere-showcase", L"legacy"});
    expect(o.ok && o.atmosphere_showcase == app::AtmosphereShowcaseMode::kLegacy,
           "showcase legacy");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--atmosphere-showcase", L"ocean"});
    expect(o.ok && o.atmosphere_showcase == app::AtmosphereShowcaseMode::kOcean,
           "showcase ocean");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--map2d-showcase", L"china"});
    expect(o.ok && o.map2d_showcase == app::Map2dShowcaseMode::kChina,
           "map2d showcase china");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--map2d-showcase=china"});
    expect(o.ok && o.map2d_showcase == app::Map2dShowcaseMode::kChina,
           "map2d showcase=china");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--map2d-showcase=align"});
    expect(o.ok && o.map2d_showcase == app::Map2dShowcaseMode::kAlign,
           "map2d showcase=align");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--map2d-showcase=orthogrid"});
    expect(o.ok && o.map2d_showcase == app::Map2dShowcaseMode::kOrthogrid,
           "map2d showcase=orthogrid");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=world3d"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kWorld3d,
           "plugin showcase=world3d");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--plugin-showcase", L"print"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kPrint,
           "plugin showcase print");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=orthogrid"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kOrthogrid,
           "plugin showcase=orthogrid");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=orthogrid3d"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kOrthogrid3d,
           "plugin showcase=orthogrid3d");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=traffic"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kTraffic,
           "plugin showcase=traffic");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--plugin-showcase", L"flood"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kFlood,
           "plugin showcase flood");
  }
  {
    auto o =
        parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=stormsurge"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kStormSurge,
           "plugin showcase=stormsurge");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=mine"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kMine,
           "plugin showcase=mine");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--plugin-showcase=geochem"});
    expect(o.ok && o.plugin_showcase == app::PluginShowcaseMode::kGeochem,
           "plugin showcase=geochem");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--ui-showcase", L"shell"});
    expect(o.ok && o.ui_showcase == app::UiShowcaseMode::kShell,
           "ui-showcase shell");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--ui-showcase=data"});
    expect(o.ok && o.ui_showcase == app::UiShowcaseMode::kData,
           "ui-showcase data");
  }
  {
    auto o = parse_vec({L"SmartGisViews.exe", L"--ui-showcase=interact"});
    expect(o.ok && o.ui_showcase == app::UiShowcaseMode::kInteract,
           "ui-showcase interact");
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
