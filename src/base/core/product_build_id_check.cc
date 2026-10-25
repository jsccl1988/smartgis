// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/core/module_build_id.h"

#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#include "base/core/log.h"

namespace base {
namespace {

// Keep in sync with build/tools/check_dll_homogeneity.py PRODUCT_STEMS.
constexpr const char* kProductStems[] = {
    "base",
    "net",
    "gis",
    "vista",
    "scenic",
    "render",
    "content",
    "tool",
    "ui_views",
    "plugin_host",
    "ui_legacy",
    "legacy_render",
    "legacy_render_gl",
    "legacy_render_d3d",
    "legacy_tool",
};

bool stem_matches_module(const wchar_t* module_base) {
  if (!module_base || !module_base[0]) {
    return false;
  }
  // basename only (Toolhelp gives that).
  char narrow[MAX_PATH];
  const int n = ::WideCharToMultiByte(CP_UTF8, 0, module_base, -1, narrow,
                                      static_cast<int>(sizeof(narrow)), nullptr,
                                      nullptr);
  if (n <= 1) {
    return false;
  }
  std::string name(narrow);
  for (char& c : name) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  constexpr const char kDll[] = ".dll";
  constexpr const char kDebug[] = "_d.dll";
  std::string stem;
  if (name.size() > sizeof(kDebug) - 1 &&
      name.compare(name.size() - (sizeof(kDebug) - 1), sizeof(kDebug) - 1,
                   kDebug) == 0) {
    stem = name.substr(0, name.size() - (sizeof(kDebug) - 1));
  } else if (name.size() > sizeof(kDll) - 1 &&
             name.compare(name.size() - (sizeof(kDll) - 1), sizeof(kDll) - 1,
                          kDll) == 0) {
    stem = name.substr(0, name.size() - (sizeof(kDll) - 1));
  } else {
    return false;
  }
  for (const char* s : kProductStems) {
    if (stem == s) {
      return true;
    }
  }
  return false;
}

}  // namespace

const char* module_build_id() {
  return smartgis_module_build_id();
}

bool verify_product_dll_build_ids() {
#if !defined(COMPONENT_BUILD)
  return true;
#else
  const char* expect = smartgis_module_build_id();
  if (!expect || !expect[0]) {
    LOGGING(LOG_ERROR, "build_id: empty id in this image");
    return false;
  }

  HANDLE snap =
      ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                 ::GetCurrentProcessId());
  if (snap == INVALID_HANDLE_VALUE) {
    LOGGING(LOG_ERROR, "build_id: CreateToolhelp32Snapshot failed (%lu)",
            ::GetLastError());
    return false;
  }

  MODULEENTRY32W me = {};
  me.dwSize = sizeof(me);
  bool ok = true;
  int checked = 0;
  if (::Module32FirstW(snap, &me)) {
    do {
      if (!stem_matches_module(me.szModule)) {
        continue;
      }
      using Fn = const char* (*)();
      const auto fn = reinterpret_cast<Fn>(
          ::GetProcAddress(me.hModule, "smartgis_module_build_id"));
      if (!fn) {
        LOGGING(LOG_ERROR,
                "build_id: product module %ls missing "
                "smartgis_module_build_id (stale/foreign DLL?)",
                me.szModule);
        ok = false;
        break;
      }
      const char* got = fn();
      if (!got || std::strcmp(got, expect) != 0) {
        LOGGING(LOG_ERROR,
                "build_id: mismatch module=%ls expect=%s got=%s — rebuild "
                "this out/<config> (do not mix DLLs from another build)",
                me.szModule, expect, got ? got : "(null)");
        ok = false;
        break;
      }
      ++checked;
    } while (::Module32NextW(snap, &me));
  }
  ::CloseHandle(snap);

  if (ok) {
    LOGGING(LOG_INFO, "build_id: OK id=%s product_modules=%d", expect, checked);
  }
  return ok;
#endif  // COMPONENT_BUILD
}

}  // namespace base
