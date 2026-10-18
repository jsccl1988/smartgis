// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/runtime.h"

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
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "smartgis_script_main_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  plugin::PythonRuntime py;
#if defined(HAS_PYTHON)
  if (!py.init()) {
    std::puts("script_main_test: skip (python failed to start)");
    return 0;
  }
#endif

  expect(write_text(dir / "no_main.py", "x = 1\n"), "write no_main");
  expect(write_text(dir / "nested_main.py",
                    "def helper():\n"
                    "    def main():\n"
                    "        return None\n"),
         "write nested_main");
  expect(write_text(dir / "has_main.py",
                    "def main():\n"
                    "    return None\n"),
         "write has_main");

  std::string error;
  expect(!py.exec_file((dir / "no_main.py").string(), &error), "no main fails");
  expect(error.find("missing callable main") != std::string::npos,
         "no main names main");

  error.clear();
  expect(!py.exec_file((dir / "nested_main.py").string(), &error),
         "nested main fails");
  expect(error.find("missing callable main") != std::string::npos,
         "nested main names main");

  error.clear();
  const bool ran = py.exec_file((dir / "has_main.py").string(), &error);
#if defined(HAS_PYTHON)
  expect(ran, "callable main runs");
  if (!ran) {
    std::fprintf(stderr, "main error: %s\n", error.c_str());
  }
  error.clear();
  expect(write_text(dir / "boom.py",
                    "def main():\n"
                    "    raise RuntimeError('boom-main')\n"),
         "write boom");
  expect(!py.exec_file((dir / "boom.py").string(), &error), "main raise fails");
  expect(error.find("boom-main") != std::string::npos, "main raise text");
  py.shutdown();
#else
  expect(!ran, "debug stub does not execute");
  expect(error.find("python not compiled in") != std::string::npos,
         "stub reports after main is present");
  std::puts(error.c_str());
  (void)ran;
#endif

  fs::remove_all(dir, ec);
  if (g_fails) {
    std::fprintf(stderr, "script_main_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("script_main_test: ok");
  return 0;
}
