// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_RECEIVER_H
#define BASE_IPC_RECEIVER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "base/ipc/channel.h"
#include "base/ipc/handle.h"

namespace base {
namespace ipc {

class PendingReceiver;
class PendingRemote;

// Dispatch target for one Channel (Mojo Receiver, no mojom).
class MessageListener {
 public:
  virtual ~MessageListener() = default;
  virtual void on_message(const Frame& frame,
                          std::vector<uint8_t> payload,
                          std::vector<PlatformHandle> handles) = 0;
  virtual void on_disconnect() {}
};

// Process-wide IO thread (Mojo ScopedIPCSupport).
class ScopedIpcSupport {
 public:
  ScopedIpcSupport();
  ~ScopedIpcSupport();

  ScopedIpcSupport(const ScopedIpcSupport&) = delete;
  ScopedIpcSupport& operator=(const ScopedIpcSupport&) = delete;

  static bool is_active();
  static void post(std::function<void()> fn);
  // Run every task posted before this call, then return.
  static void flush();
  static void watch(HANDLE event, std::function<void()> cb);
  static void unwatch(HANDLE event);

 private:
  void run();

  struct Watch {
    HANDLE event = nullptr;
    std::function<void()> cb;
  };

  std::mutex mu_;
  std::vector<std::function<void()>> queue_;
  std::vector<Watch> watches_;
  std::atomic<bool> stop_{false};
  HANDLE wake_ = nullptr;
  std::thread thread_;
};

// Watches a Channel on the IO thread and dispatches to `listener`.
class Receiver {
 public:
  Receiver(Channel* channel, MessageListener* listener);
  Receiver(PendingReceiver pending, MessageListener* listener);
  ~Receiver();

  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;

 private:
  struct State;
  void start(Channel* channel, MessageListener* listener);
  static void pump(std::shared_ptr<State> state);

  Channel owned_;
  std::shared_ptr<State> state_;
  std::thread private_thread_;
};

// Thin send helper (Mojo Remote, no generated stubs).
class Remote {
 public:
  explicit Remote(Channel* channel);
  explicit Remote(PendingRemote pending);

  template <typename T>
  bool send(uint16_t type, uint32_t view_id, const T& body) {
    return channel() && channel()->send_msg(type, view_id, body);
  }

  bool send_empty(uint16_t type, uint32_t view_id) {
    return channel() && channel()->send_empty(type, view_id);
  }

  uint32_t next_seq();

 private:
  Channel* channel();

  Channel owned_;
  Channel* ptr_ = nullptr;
};

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_RECEIVER_H
