// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_
#define CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_

#include <functional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/app/process_type.h"

namespace content {
namespace detail {

// Relaunches this executable as --type= under a job that kills the child
// when the job handle closes. Pipe Hello stays with the caller.
class ChildProcessHost {
 public:
  enum class Status {
    kOk,
    kCreateFailed,
    kWaitFailed,
  };

  // Job with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE. Null when the OS refuses.
  // The caller closes the handle.
  static HANDLE open_job();

  // Same PE, --type= / --parent-pid= / --pipe= / --session=, then CreateProcess,
  // assign |job|, and |wait_client| (pipe connect, not Hello).
  // On kOk, |process| receives the process handle and the thread is closed.
  // On kWaitFailed the process handle is already closed and |process| is null.
  static Status launch(ProcessType type,
                       const std::wstring& pipe_name,
                       HANDLE job,
                       const std::function<bool()>& wait_client,
                       HANDLE* process);
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_
