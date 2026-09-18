// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_ENDPOINT_H
#define BASE_IPC_ENDPOINT_H

#include "base/ipc/channel.h"
#include "base/ipc/invitation.h"

namespace base {
namespace ipc {

// Movable message-pipe end (mojom PendingRemote shape, no IDL).
class PendingRemote {
 public:
  PendingRemote() = default;
  explicit PendingRemote(Channel&& channel) : channel_(std::move(channel)) {}

  bool is_valid() const { return channel_.is_open(); }
  Channel take_channel() { return std::move(channel_); }

 private:
  Channel channel_;
};

// Movable message-pipe end (mojom PendingReceiver shape, no IDL).
class PendingReceiver {
 public:
  PendingReceiver() = default;
  explicit PendingReceiver(Channel&& channel) : channel_(std::move(channel)) {}

  bool is_valid() const { return channel_.is_open(); }
  Channel take_channel() { return std::move(channel_); }

 private:
  Channel channel_;
};

// Creates a connected PendingRemote / PendingReceiver pair (no mojom).
struct InterfaceEndpoint {
  static bool create_pair(PendingRemote* remote, PendingReceiver* receiver);
};

inline bool InterfaceEndpoint::create_pair(PendingRemote* remote,
                                           PendingReceiver* receiver) {
  if (!remote || !receiver) {
    return false;
  }
  PlatformChannel local;
  PlatformChannel peer;
  if (!PlatformChannel::create_pair(&local, &peer)) {
    return false;
  }
  Channel a;
  Channel b;
  if (!a.adopt(local.release()) || !b.adopt(peer.release())) {
    return false;
  }
  *remote = PendingRemote(std::move(a));
  *receiver = PendingReceiver(std::move(b));
  return true;
}

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_ENDPOINT_H
