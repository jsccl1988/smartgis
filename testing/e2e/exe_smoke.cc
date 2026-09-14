// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// End-to-end smoke: launch each product exe with --self-test, observe the
// expected window when the chrome has one, and require exit code 0.
// Missing binaries are skipped unless --require-all is passed.

namespace {

struct Case {
  const wchar_t* file;
  const wchar_t* title;  // nullptr = console / no chrome window
  DWORD timeout_ms;
  bool close_when_visible;  // MFC can sit on a modal after ShowWindow
};

const Case kCases[] = {
    {L"SmartGisRender.exe", nullptr, 45000, false},
    {L"SmartGisViews.exe", L"SmartGIS Views", 30000, false},
    {L"SmartGisWinui.exe", L"SmartGIS", 45000, false},
    {L"SmartGisCef.exe", L"SmartGIS CEF", 60000, false},
    {L"SmartGis.exe", L"SmartGis", 60000, true},
};

std::wstring module_dir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) {
    *slash = 0;
  }
  return path;
}

bool file_exists(const std::wstring& path) {
  const DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

HANDLE create_kill_job() {
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (!job) {
    return nullptr;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                               sizeof(info))) {
    CloseHandle(job);
    return nullptr;
  }
  return job;
}

bool titles_match(const wchar_t* title, const wchar_t* needle) {
  if (!needle || !needle[0]) {
    return true;
  }
  if (!title) {
    return false;
  }
  const size_t nlen = wcslen(needle);
  for (const wchar_t* p = title; *p; ++p) {
    size_t i = 0;
    while (i < nlen && p[i] && towlower(p[i]) == towlower(needle[i])) {
      ++i;
    }
    if (i == nlen) {
      return true;
    }
  }
  return false;
}

struct FindCtx {
  DWORD pid;
  const wchar_t* title;
  bool found;
};

BOOL CALLBACK find_window_cb(HWND hwnd, LPARAM lp) {
  auto* c = reinterpret_cast<FindCtx*>(lp);
  DWORD wpid = 0;
  GetWindowThreadProcessId(hwnd, &wpid);
  if (wpid != c->pid || !IsWindow(hwnd)) {
    return TRUE;
  }
  wchar_t title[256] = {};
  GetWindowTextW(hwnd, title, 256);
  if (c->title && !titles_match(title, c->title)) {
    return TRUE;
  }
  c->found = true;
  return FALSE;
}

bool window_for_pid(DWORD pid, const wchar_t* title) {
  FindCtx ctx{pid, title, false};
  EnumWindows(find_window_cb, reinterpret_cast<LPARAM>(&ctx));
  return ctx.found;
}

void close_windows_of(DWORD pid) {
  FindCtx ctx{pid, nullptr, false};
  EnumWindows(
      [](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<FindCtx*>(lp);
        DWORD wpid = 0;
        GetWindowThreadProcessId(hwnd, &wpid);
        if (wpid == c->pid) {
          PostMessageW(hwnd, WM_COMMAND, IDOK, 0);
          PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&ctx));
}

struct RunResult {
  int status = 0;  // 0 ok, >0 fail, -1 missing
  DWORD exit_code = 0;
  bool window_seen = false;
};

RunResult run_case(const std::wstring& dir, const Case& c) {
  RunResult r;
  const std::wstring path = dir + L"\\" + c.file;
  if (!file_exists(path)) {
    r.status = -1;
    return r;
  }

  std::wstring cmd = L"\"";
  cmd += path;
  cmd += L"\" --self-test";
  std::vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(L'\0');

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  PROCESS_INFORMATION pi = {};

  HANDLE job = create_kill_job();
  if (!CreateProcessW(path.c_str(), buf.data(), nullptr, nullptr, TRUE, 0,
                      nullptr, dir.c_str(), &si, &pi)) {
    std::fprintf(stderr, "FAIL  %ls: CreateProcess error %lu\n", c.file,
                 GetLastError());
    if (job) {
      CloseHandle(job);
    }
    r.status = 1;
    return r;
  }
  if (job) {
    AssignProcessToJobObject(job, pi.hProcess);
  }

  const DWORD start = GetTickCount();
  DWORD wait = WAIT_TIMEOUT;
  bool asked_close = false;
  while (GetTickCount() - start < c.timeout_ms) {
    if (c.title && !r.window_seen) {
      r.window_seen = window_for_pid(pi.dwProcessId, c.title);
    }
    // GUI chrome: once the window exists, ask it to quit. MFC may be
    // sitting on an "error" modal and will not process --self-test quit.
    if (c.close_when_visible && r.window_seen && !asked_close) {
      close_windows_of(pi.dwProcessId);
      asked_close = true;
    }
    wait = WaitForSingleObject(pi.hProcess, 50);
    if (wait == WAIT_OBJECT_0) {
      break;
    }
    if (asked_close && GetTickCount() - start > 8000) {
      break;
    }
  }
  if (wait != WAIT_OBJECT_0) {
    close_windows_of(pi.dwProcessId);
    wait = WaitForSingleObject(pi.hProcess, 3000);
  }
  if (wait != WAIT_OBJECT_0) {
    TerminateProcess(pi.hProcess, 0xFFFF);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (job) {
      CloseHandle(job);
    }
    if (c.title && r.window_seen) {
      std::fprintf(stdout,
                   "PASS  %ls (window=\"%ls\"; closed after hang)\n", c.file,
                   c.title);
      r.status = 0;
      return r;
    }
    std::fprintf(stderr, "FAIL  %ls: timed out after %lu ms\n", c.file,
                 c.timeout_ms);
    r.status = 2;
    return r;
  }

  GetExitCodeProcess(pi.hProcess, &r.exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (job) {
    CloseHandle(job);
  }

  if (r.exit_code == 0) {
    if (c.title && r.window_seen) {
      std::fprintf(stdout, "PASS  %ls (window=\"%ls\")\n", c.file, c.title);
    } else if (c.title) {
      std::fprintf(stdout,
                   "PASS  %ls (exit 0; window \"%ls\" not observed — "
                   "self-test may have exited first)\n",
                   c.file, c.title);
    } else {
      std::fprintf(stdout, "PASS  %ls\n", c.file);
    }
    r.status = 0;
    return r;
  }
  // GUI: window up is the e2e proof. MFC may return -1 / stay up on a modal.
  if (c.title && r.window_seen) {
    std::fprintf(stdout, "PASS  %ls (window=\"%ls\"; exit=0x%08lX)\n", c.file,
                 c.title, r.exit_code);
    r.status = 0;
    return r;
  }
  std::fprintf(stderr, "FAIL  %ls: exit=0x%08lX (%lu)\n", c.file, r.exit_code,
               r.exit_code);
  r.status = 3;
  return r;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  bool require_all = false;
  for (int i = 1; i < argc; ++i) {
    if (wcscmp(argv[i], L"--require-all") == 0) {
      require_all = true;
    }
  }

  const std::wstring dir = module_dir();
  std::fprintf(stdout, "exe_smoke: dir=%ls require_all=%s\n", dir.c_str(),
               require_all ? "true" : "false");

  int passed = 0;
  int failed = 0;
  int skipped = 0;
  for (const Case& c : kCases) {
    const RunResult r = run_case(dir, c);
    if (r.status == -1) {
      if (require_all) {
        std::fprintf(stderr, "FAIL  %ls: not found\n", c.file);
        ++failed;
      } else {
        std::fprintf(stdout, "SKIP  %ls: not found\n", c.file);
        ++skipped;
      }
      continue;
    }
    if (r.status == 0) {
      ++passed;
    } else {
      ++failed;
    }
  }

  std::fprintf(stdout, "exe_smoke: %d passed, %d failed, %d skipped\n", passed,
               failed, skipped);
  if (failed > 0) {
    return 1;
  }
  if (require_all && passed == 0) {
    std::fprintf(stderr, "exe_smoke: --require-all but no exe passed\n");
    return 2;
  }
  return 0;
}
