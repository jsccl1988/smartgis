// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/receiver.h"

#include "base/ipc/endpoint.h"

#include <algorithm>

namespace base {
namespace ipc {
namespace {

ScopedIpcSupport* g_support = nullptr;

}  // namespace

struct Receiver::State {
  std::atomic<bool> stop{false};
  Channel* channel = nullptr;
  MessageListener* listener = nullptr;
};

ScopedIpcSupport::ScopedIpcSupport() {
  wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  g_support = this;
  thread_ = std::thread(&ScopedIpcSupport::run, this);
}

ScopedIpcSupport::~ScopedIpcSupport() {
  stop_ = true;
  if (wake_) {
    SetEvent(wake_);
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  if (g_support == this) {
    g_support = nullptr;
  }
  if (wake_) {
    CloseHandle(wake_);
    wake_ = nullptr;
  }
}

bool ScopedIpcSupport::is_active() {
  return g_support != nullptr;
}

void ScopedIpcSupport::post(std::function<void()> fn) {
  if (!g_support || !fn || g_support->stop_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(g_support->mu_);
    g_support->queue_.push_back(std::move(fn));
  }
  SetEvent(g_support->wake_);
}

void ScopedIpcSupport::flush() {
  if (!g_support) {
    return;
  }
  HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!done) {
    return;
  }
  post([done] { SetEvent(done); });
  WaitForSingleObject(done, 15000);
  CloseHandle(done);
}

void ScopedIpcSupport::watch(HANDLE event, std::function<void()> cb) {
  if (!g_support || !event || !cb || g_support->stop_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(g_support->mu_);
    for (auto& w : g_support->watches_) {
      if (w.event == event) {
        w.cb = std::move(cb);
        SetEvent(g_support->wake_);
        return;
      }
    }
    g_support->watches_.push_back({event, std::move(cb)});
  }
  SetEvent(g_support->wake_);
}

void ScopedIpcSupport::unwatch(HANDLE event) {
  if (!g_support || !event) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(g_support->mu_);
    auto& ws = g_support->watches_;
    ws.erase(std::remove_if(ws.begin(), ws.end(),
                            [event](const Watch& w) { return w.event == event; }),
             ws.end());
  }
  SetEvent(g_support->wake_);
}

void ScopedIpcSupport::run() {
  while (!stop_) {
    HANDLE objs[64] = {};
    std::function<void()> cbs[63];
    DWORD n = 1;
    objs[0] = wake_;
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (const auto& w : watches_) {
        if (n >= 64 || !w.event) {
          break;
        }
        objs[n] = w.event;
        cbs[n - 1] = w.cb;
        ++n;
      }
    }
    const DWORD w = WaitForMultipleObjects(n, objs, FALSE, INFINITE);
    if (w == WAIT_FAILED) {
      Sleep(1);
      continue;
    }
    std::vector<std::function<void()>> batch;
    {
      std::lock_guard<std::mutex> lock(mu_);
      batch.swap(queue_);
    }
    for (auto& fn : batch) {
      if (fn) {
        fn();
      }
    }
    for (DWORD i = 1; i < n; ++i) {
      if (WaitForSingleObject(objs[i], 0) == WAIT_OBJECT_0) {
        if (cbs[i - 1]) {
          cbs[i - 1]();
        }
      }
    }
  }
}

void Receiver::start(Channel* channel, MessageListener* listener) {
  state_ = std::make_shared<State>();
  state_->channel = channel;
  state_->listener = listener;
  if (!channel || !listener) {
    return;
  }
  if (ScopedIpcSupport::is_active()) {
    channel->arm_header_read();
    ScopedIpcSupport::watch(channel->readable_event(),
                            [st = state_] { pump(st); });
    if (channel->has_ready_header()) {
      ScopedIpcSupport::post([st = state_] { pump(st); });
    }
    return;
  }
  private_thread_ = std::thread([st = state_] {
    while (st && !st->stop && st->channel && st->channel->is_open()) {
      pump(st);
    }
  });
}

Receiver::Receiver(Channel* channel, MessageListener* listener) {
  start(channel, listener);
}

Receiver::Receiver(PendingReceiver pending, MessageListener* listener) {
  owned_ = pending.take_channel();
  start(&owned_, listener);
}

Receiver::~Receiver() {
  if (state_) {
    state_->stop = true;
    state_->listener = nullptr;
  }
  if (state_ && state_->channel && ScopedIpcSupport::is_active()) {
    ScopedIpcSupport::unwatch(state_->channel->readable_event());
  }
  if (state_ && state_->channel) {
    state_->channel->interrupt();
  }
  if (ScopedIpcSupport::is_active()) {
    ScopedIpcSupport::flush();
  }
  if (private_thread_.joinable()) {
    private_thread_.join();
  }
  owned_.close();
}

void Receiver::pump(std::shared_ptr<State> state) {
  if (!state || state->stop || !state->channel || !state->listener) {
    return;
  }
  const uint32_t timeout =
      ScopedIpcSupport::is_active() ? 0 : 250;
  Frame frame;
  std::vector<uint8_t> payload;
  std::vector<PlatformHandle> handles;
  if (state->channel->recv(&frame, &payload, &handles, timeout)) {
    if (!state->stop && state->listener) {
      state->listener->on_message(frame, std::move(payload), std::move(handles));
    }
  } else if (!state->channel->is_open()) {
    if (!state->stop && state->listener) {
      state->listener->on_disconnect();
    }
    return;
  }
  if (state->stop) {
    return;
  }
  if (ScopedIpcSupport::is_active()) {
    state->channel->arm_header_read();
    if (state->channel->has_ready_header()) {
      ScopedIpcSupport::post([state] { pump(state); });
    }
    return;
  }
}

Remote::Remote(Channel* channel) : ptr_(channel) {}

Remote::Remote(PendingRemote pending) : owned_(pending.take_channel()) {
  ptr_ = &owned_;
}

Channel* Remote::channel() {
  return ptr_;
}

uint32_t Remote::next_seq() {
  return ptr_ ? ptr_->next_seq() : 0;
}

}  // namespace ipc
}  // namespace base
