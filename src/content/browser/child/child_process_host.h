// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_
#define CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "common/ipc/channel/channel.h"
#include "content/app/process_type.h"

namespace content {
namespace detail {

// Relaunches this executable as --type= under a job that kills the child
// when the job handle closes. Bootstrap is common::ipc invitation
// (--ipc-channel-handle=); product path does not use --pipe=.
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

  // Same PE, --type= / --parent-pid= / --session= / --ipc-channel-handle=,
  // then invitation attach(|attach_name|). On kOk, |channel| is the local end
  // and |process| receives the process handle (thread closed).
  // |attach_name| is typically "gpu" or "renderer".
  static Status launch(ProcessType type,
                       HANDLE job,
                       const char* attach_name,
                       common::ipc::Channel* channel,
                       HANDLE* process);
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_CHILD_PROCESS_HOST_H_
