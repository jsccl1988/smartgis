// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/detail/bootstrap.h"

#include <cstdio>
#include <string>

#include <MddBootstrap.h>
#include <WindowsAppSDK-VersionInfo.h>

namespace app {
namespace winui {
namespace detail {
namespace {

bool g_bootstrapped = false;

void print_bootstrap_error(HRESULT hr) {
  wchar_t buf[768];
  swprintf_s(buf,
             L"SmartGisWinui.exe: Windows App SDK bootstrap failed (hr=0x%08X).\n"
             L"Unpackaged Win32 needs the Windows App Runtime matching "
             L"Microsoft.WindowsAppSDK " WINDOWSAPPSDK_RELEASE_VERSION_TAG_W
             L" (or the version in third_party/windows_app_sdk/VERSION).\n"
             L"Install the official Windows App Runtime redistributable, or "
             L"extract Microsoft.WindowsAppSDK from nuget.org into "
             L"third_party/windows_app_sdk (not third_party/.install).\n",
             static_cast<unsigned>(hr));
  OutputDebugStringW(buf);
  fwprintf(stderr, L"%s", buf);
  MessageBoxW(nullptr, buf, L"SmartGIS WinUI — missing Windows App SDK",
              MB_OK | MB_ICONERROR);
}

}  // namespace

bool initialize_windows_app_sdk() {
  const UINT32 major_minor = WINDOWSAPPSDK_RELEASE_MAJORMINOR;
  const PCWSTR tag = WINDOWSAPPSDK_RELEASE_VERSION_TAG_W;
  PACKAGE_VERSION min_version{};
  min_version.Major = WINDOWSAPPSDK_RUNTIME_VERSION_MAJOR;
  min_version.Minor = WINDOWSAPPSDK_RUNTIME_VERSION_MINOR;
  min_version.Build = WINDOWSAPPSDK_RUNTIME_VERSION_BUILD;
  min_version.Revision = WINDOWSAPPSDK_RUNTIME_VERSION_REVISION;

  const HRESULT hr = MddBootstrapInitialize2(
      major_minor, tag, min_version,
      MddBootstrapInitializeOptions_OnNoMatch_ShowUI);
  if (FAILED(hr)) {
    print_bootstrap_error(hr);
    return false;
  }
  g_bootstrapped = true;
  return true;
}

void shutdown_windows_app_sdk() {
  if (g_bootstrapped) {
    MddBootstrapShutdown();
    g_bootstrapped = false;
  }
}

}  // namespace detail
}  // namespace winui
}  // namespace app
