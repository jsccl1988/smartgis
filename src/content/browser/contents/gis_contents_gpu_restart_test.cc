// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/content_main.h"
#include "content/public/gis_contents.h"
#include "content/public/widget_host_view.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

struct Host : content::GisContentsClient {
  int browser_main(const content::ContentMainParams&) override { return 0; }
};

bool command_line_has_type_gpu() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--type=gpu") != nullptr;
}

}  // namespace

int main() {
  if (command_line_has_type_gpu()) {
    // Keep the full parent-supplied argv so --ipc-channel-handle= survives.
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 1) {
      return 2;
    }
    Host host;
    content::ContentMainParams params;
    params.argc = argc;
    params.argv = argv;
    params.process_type = content::ProcessType::kGpu;
    params.process_type_set = true;
    const int rc = content::content_main(params, host);
    LocalFree(argv);
    return rc;
  }

  std::unique_ptr<content::GisContents> contents(content::create_gis_contents());
  expect(contents != nullptr, "create_gis_contents");
  if (!contents) {
    return 1;
  }

  expect(contents->StartRenderProcess(), "StartRenderProcess");
  expect(contents->IsOopRender(), "IsOopRender after start");
  if (!contents->IsOopRender()) {
    contents->Shutdown();
    return g_fails ? 1 : 0;
  }

  const uint32_t view =
      contents->OpenView(content::ViewKind::kMapEdit);
  expect(view != 0, "OpenView");
  content::WidgetHostView* host =
      contents->AttachSurface(view, content::PresentMode::kSharedTexture);
  expect(host != nullptr, "AttachSurface");
  if (host) {
    host->Resize(256, 256, 96.f);
  }

  expect(contents->WaitFrameReady(view, 15000), "WaitFrameReady first frame");

  content::SharedSurface before = host ? host->Latest() : content::SharedSurface{};
  expect(before.generation != 0 || before.nt_handle != nullptr,
         "Latest surface after first frame");

  // Kill the GPU child. GisContentsImpl must relaunch and re-attach.
  // Process handle is not on the public API — terminate by job of children
  // via a second Start is not available; send Shutdown to child pipe by
  // closing through a forced disconnect: open a throwaway contents? Instead
  // use toolhelp to find child of this PID with same image.
  {
    DWORD parent = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    expect(snap != INVALID_HANDLE_VALUE, "CreateToolhelp32Snapshot");
    if (snap != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32W pe = {};
      pe.dwSize = sizeof(pe);
      bool killed = false;
      if (Process32FirstW(snap, &pe)) {
        do {
          if (pe.th32ParentProcessID == parent &&
              pe.th32ProcessID != parent) {
            HANDLE child =
                OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                            pe.th32ProcessID);
            if (child) {
              TerminateProcess(child, 42);
              WaitForSingleObject(child, 5000);
              CloseHandle(child);
              killed = true;
              break;
            }
          }
        } while (Process32NextW(snap, &pe));
      }
      CloseHandle(snap);
      expect(killed, "terminated GPU child process");
    }
  }

  // Allow async restart thread + Hello + re-paint.
  bool recovered = false;
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < 20000) {
    if (contents->IsOopRender() &&
        contents->WaitFrameReady(view, 500)) {
      content::SharedSurface after =
          host ? host->Latest() : content::SharedSurface{};
      if (after.generation != 0 || after.nt_handle != nullptr) {
        recovered = true;
        break;
      }
    }
    Sleep(50);
  }
  expect(recovered, "WaitFrameReady after GPU kill/restart");
  expect(contents->IsOopRender(), "still OOP after restart");

  contents->Shutdown();

  if (g_fails != 0) {
    std::fprintf(stderr, "gis_contents_gpu_restart_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("gis_contents_gpu_restart_test: ok\n");
  return 0;
}
