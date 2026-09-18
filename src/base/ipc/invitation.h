// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_INVITATION_H
#define BASE_IPC_INVITATION_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/ipc/channel.h"
#include "base/ipc/handle.h"

namespace base {
namespace ipc {

inline constexpr uint16_t k_msg_invitation = 0;
inline constexpr wchar_t k_channel_handle_switch[] = L"--ipc-channel-handle=";

// One end of a pre-connected duplex named pipe (Mojo PlatformChannel).
class PlatformChannel {
 public:
  PlatformChannel() = default;
  explicit PlatformChannel(HANDLE handle);
  ~PlatformChannel();

  PlatformChannel(const PlatformChannel&) = delete;
  PlatformChannel& operator=(const PlatformChannel&) = delete;
  PlatformChannel(PlatformChannel&& other) noexcept;
  PlatformChannel& operator=(PlatformChannel&& other) noexcept;

  static bool create_pair(PlatformChannel* local, PlatformChannel* remote);
  static PlatformChannel from_command_line(int argc, wchar_t** argv);

  bool is_valid() const;
  HANDLE get() const { return handle_; }
  HANDLE release();
  void close();

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

struct InvitationPipeRec {
  std::string name;
  uint64_t handle_token = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(name, handle_token);
  }
};

// Bootstrap payload: up to two named pipes (renderer / gpu).
struct InvitationBody {
  uint32_t version = 1;
  InvitationPipeRec pipe0;
  InvitationPipeRec pipe1;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(version, pipe0, pipe1);
  }
};

class IncomingInvitation;

// Parent-side invitation (Mojo OutgoingInvitation).
class OutgoingInvitation {
 public:
  OutgoingInvitation();
  ~OutgoingInvitation();

  OutgoingInvitation(const OutgoingInvitation&) = delete;
  OutgoingInvitation& operator=(const OutgoingInvitation&) = delete;

  Channel attach(const std::string& name);
  bool send_to(HANDLE target_process);
  // Same-process handoff (tests / --in-process-gpu): send_to(self) + accept.
  bool handoff_local(IncomingInvitation* incoming);
  HANDLE bootstrap_remote() const { return bootstrap_remote_.get(); }
  void close_bootstrap_remote() { bootstrap_remote_.close(); }

 private:
  Channel bootstrap_;
  PlatformChannel bootstrap_remote_;
  std::vector<std::pair<std::string, PlatformChannel>> attached_;
};

// Child-side invitation (Mojo IncomingInvitation).
class IncomingInvitation {
 public:
  IncomingInvitation() = default;

  static IncomingInvitation accept(PlatformChannel channel,
                                   uint32_t timeout_ms = 15000);
  static IncomingInvitation accept(int argc,
                                   wchar_t** argv,
                                   uint32_t timeout_ms = 15000);

  Channel extract(const std::string& name);

 private:
  InvitationBody body_{};
};

struct ChildLaunch {
  std::wstring exe;
  std::wstring extra_args;
  std::wstring cwd;
  HANDLE job = nullptr;
};

// CreateProcess with PROC_THREAD_ATTRIBUTE_HANDLE_LIST, then send_to(child).
bool launch_with_invitation(OutgoingInvitation* invitation,
                            const ChildLaunch& launch,
                            PROCESS_INFORMATION* pi);

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_INVITATION_H
