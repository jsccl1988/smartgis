// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/child/child_process_host.h"

#include <cstdio>
#include <string>

#include "common/ipc/invitation/invitation.h"
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
    HANDLE job,
    const char* attach_name,
    common::ipc::Channel* channel,
    HANDLE* process) {
  if (process) {
    *process = nullptr;
  }
  if (!process || !channel || !attach_name || !attach_name[0]) {
    return Status::kCreateFailed;
  }

  const uint32_t pid = GetCurrentProcessId();
  wchar_t pid_buf[32];
  swprintf_s(pid_buf, L"%u", pid);

  std::wstring extra = L"--type=";
  extra += ProcessTypeSwitchValue(type);
  extra += L" --parent-pid=";
  extra += pid_buf;
  extra += L" --session=";
  extra += make_session_id();
  base::append_switches_to_command_line(&extra);

  common::ipc::OutgoingInvitation invitation;
  common::ipc::Channel local = invitation.attach(attach_name);
  if (!local.is_open()) {
    return Status::kCreateFailed;
  }

  common::ipc::ChildLaunch launch;
  launch.exe = this_exe_path();
  launch.extra_args = extra;
  launch.cwd = module_dir();
  launch.job = job;

  PROCESS_INFORMATION pi = {};
  if (!common::ipc::launch_with_invitation(&invitation, launch, &pi)) {
    return Status::kCreateFailed;
  }

  *channel = std::move(local);
  if (pi.hThread) {
    CloseHandle(pi.hThread);
  }
  *process = pi.hProcess;
  return Status::kOk;
}

}  // namespace detail
}  // namespace content
