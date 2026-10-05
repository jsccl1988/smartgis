// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/python/runtime.h"
#include "plugin/runtime/host/registry/registry.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

#ifndef PLUGIN_HELLO_DIR
#define PLUGIN_HELLO_DIR ""
#endif

bool write_text(const std::filesystem::path& path, const char* body) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out << body;
  return static_cast<bool>(out);
}

}  // namespace

int main() {
  plugin::PythonRuntime py;
  if (!py.init()) {
    std::puts("plugin_python_test: skip (no python)");
    return 0;
  }

  namespace fs = std::filesystem;
  const fs::path tmp = fs::temp_directory_path() / "smartgis_plugin_python";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp / "ok");
  fs::create_directories(tmp / "boom");

  expect(write_text(tmp / "ok" / "plugin.py",
                    "def start(host):\n"
                    "    def _run(args):\n"
                    "        return True\n"
                    "    def _dlg():\n"
                    "        return None\n"
                    "    def _proc(args_json):\n"
                    "        return args_json == '{}'\n"
                    "    host.contribute_command('smartgis.sample_hello',"
                    " 'sample.hello', 'Hello', 'tools', _run)\n"
                    "    host.contribute_dialog('smartgis.sample_hello',"
                    " 'sample.dlg', 'Dlg', _dlg)\n"
                    "    host.contribute_dock('smartgis.sample_hello',"
                    " 'sample.dock', 'Dock', 'right', _dlg)\n"
                    "    host.contribute_processing('smartgis.sample_hello',"
                    " 'sample.proc', 'Proc', _proc)\n"
                    "def stop():\n"
                    "    pass\n"),
         "write ok plugin");
  expect(write_text(tmp / "boom" / "plugin.py",
                    "def start(host):\n"
                    "    raise RuntimeError('boom')\n"),
         "write boom plugin");

  content::EventBus bus;
  tool::CommandCatalog catalog;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  expect(py.start((tmp / "ok").string(), "plugin.py", host), "start ok");
  expect(host->execute("sample.hello", {}), "sample.hello");
  expect(host->open_dialog("sample.dlg"), "open sample.dlg");
  expect(host->run_processing("sample.proc", "{}"), "run sample.proc");

  {
    const std::string out = py.eval(
        "import smartgis\n"
        "print(smartgis.content.host.open_dialog('sample.dlg'))\n"
        "print(smartgis.content.host.run_processing('sample.proc', '{}'))");
    expect(out.find("True") != std::string::npos, "host open/run via eval");
  }

  ui::views::set_file_picker_modals_suppressed_for_test(true);
  ui::views::set_message_box_suppressed_for_test(true);
  {
    const std::string out = py.eval(
        "import smartgis\n"
        "r = smartgis.ui.pick_open_file('*.txt')\n"
        "smartgis.ui.show_message_box('info', 'x')\n"
        "print(repr(r))");
    expect(out.find("False") != std::string::npos, "pick_open suppressed");
  }
  ui::views::set_file_picker_modals_suppressed_for_test(false);
  ui::views::set_message_box_suppressed_for_test(false);

  {
    const std::string out = py.eval("1 + 1");
    expect(out.find("2") != std::string::npos, "eval 1+1");
  }
  {
    const std::string out = py.eval("import smartgis.gis.analysis as a; len(a.ops())");
    expect(!out.empty() && out.find("error") == std::string::npos,
           "import smartgis.gis.analysis");
  }
  {
    const std::string out = py.eval(
        "import smartgis\n"
        "smartgis.debug.set_tracing(True)\n"
        "print(smartgis.debug.tracing_enabled())\n"
        "cm = smartgis.debug.trace_event('py_test', 'plugin')\n"
        "cm.__enter__()\n"
        "cm.__exit__(None, None, None)\n"
        "smartgis.debug.set_tracing(False)\n"
        "print(smartgis.debug.tracing_enabled())");
    expect(out.find("True") != std::string::npos, "debug tracing on");
    expect(out.find("False") != std::string::npos, "debug tracing off");
  }
  {
    const std::string out = py.eval(
        "import smartgis\n"
        "print(smartgis.tool.activate('edit.append.linestring'))");
    expect(out.find("False") != std::string::npos ||
               out.find("True") != std::string::npos,
           "tool.activate no crash");
  }

  py.stop();

  expect(!py.start((tmp / "boom").string(), "plugin.py", host),
         "start raises");

  plugin::Registry reg;
  plugin::bind_registry_python(&reg, &py);
  plugin::Manifest m;
  m.id = "user.pyboom";
  m.name = "B";
  m.version = "1.0.0";
  m.api_version = 2;
  m.kind = plugin::PluginKind::kPython;
  m.entry = "plugin.py";
  expect(reg.add_manifest(m, plugin::TrustClass::kUnsignedTrusted), "add py");
  if (plugin::PluginRecord* rec =
          const_cast<plugin::PluginRecord*>(reg.find("user.pyboom"))) {
    rec->directory = (tmp / "boom").string();
  }
  expect(!reg.set_enabled("user.pyboom", true, host), "registry start fail");
  expect(reg.find("user.pyboom")->state == plugin::PluginState::kError,
         "kError after raise");

  const char* hello_dir = PLUGIN_HELLO_DIR;
  if (hello_dir && hello_dir[0]) {
    const fs::path hello(hello_dir);
    if (fs::exists(hello / "plugin.py")) {
      content::PluginHost* hello_host =
          content::create_plugin_host(&catalog, &bus, nullptr);
      expect(py.start(hello.string(), "plugin.py", hello_host),
             "hello sample start");
      expect(hello_host->execute("sample.hello", {}), "hello sample.hello");
      py.stop();
      delete hello_host;
    } else {
      std::fprintf(stderr, "hello sample missing at %s\n", hello_dir);
      ++g_fails;
    }
  }

  delete host;
  py.shutdown();
  fs::remove_all(tmp, ec);

  if (g_fails) {
    std::fprintf(stderr, "plugin_python_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("plugin_python_test: ok");
  return 0;
}
