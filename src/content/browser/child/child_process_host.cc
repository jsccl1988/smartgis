// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/child/child_process_host.h"

#include <cstdio>
#include <string>
#include <vector>

#include "base/process/switches.h"

namespace content {
namespace detail {
namespace {

std::wstring module_dir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) {
    *slash = 0;
  }
  return path;
}

std::wstring this_exe_path() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return path;
}

std::wstring make_session_id() {
  wchar_t buf[80];
  swprintf_s(buf, L"%u-%lu", GetCurrentProcessId(), GetTickCount());
  return buf;
}

}  // namespace

HANDLE ChildProcessHost::open_job() {
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (!job) {
    return nullptr;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                          sizeof(info));
  return job;
}

ChildProcessHost::Status ChildProcessHost::launch(
    ProcessType type,
    const std::wstring& pipe_name,
    HANDLE job,
    const std::function<bool()>& wait_client,
    HANDLE* process) {
  if (process) {
    *process = nullptr;
  }
  if (!process) {
    return Status::kCreateFailed;
  }

  const uint32_t pid = GetCurrentProcessId();
  const std::wstring exe = this_exe_path();
  std::wstring cmd = L"\"" + exe + L"\" --type=";
  cmd += ProcessTypeSwitchValue(type);
  wchar_t pid_buf[32];
  swprintf_s(pid_buf, L"%u", pid);
  cmd += L" --parent-pid=";
  cmd += pid_buf;
  cmd += L" --pipe=";
  cmd += pipe_name;
  cmd += L" --session=";
  cmd += make_session_id();
  base::append_switches_to_command_line(&cmd);

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
  cmd_buf.push_back(L'\0');
  if (!CreateProcessW(exe.c_str(), cmd_buf.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, module_dir().c_str(), &si,
                      &pi)) {
    return Status::kCreateFailed;
  }
  if (job) {
    AssignProcessToJobObject(job, pi.hProcess);
  }
  if (!wait_client || !wait_client()) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return Status::kWaitFailed;
  }
  if (pi.hThread) {
    CloseHandle(pi.hThread);
  }
  *process = pi.hProcess;
  return Status::kOk;
}

}  // namespace detail
}  // namespace content
