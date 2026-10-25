// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "common/ipc/proxy/channel_proxy.h"

#include <algorithm>
#include <utility>

namespace common {
namespace ipc {

struct ChannelProxy::SendState {
  Channel* channel = nullptr;
};

ChannelProxy::ChannelProxy(Channel&& channel) {
  adopt(std::move(channel));
}

ChannelProxy::~ChannelProxy() {
  reset();
}

bool ChannelProxy::adopt(Channel&& channel) {
  reset();
  channel_ = std::move(channel);
  if (!channel_.is_open()) {
    return false;
  }
  send_state_ = std::make_shared<SendState>();
  send_state_->channel = &channel_;
  return true;
}

bool ChannelProxy::bind(MessageListener* listener) {
  if (!channel_.is_open() || !listener) {
    return false;
  }
  receiver_.reset();
  receiver_ = std::make_unique<Receiver>(&channel_, listener);
  return true;
}

void ChannelProxy::reset() {
  receiver_.reset();
  if (send_state_) {
    send_state_->channel = nullptr;
    send_state_.reset();
  }
  channel_.close();
}

bool ChannelProxy::write_frame(const std::shared_ptr<SendState>& state,
                               uint16_t type,
                               uint32_t view_id,
                               uint32_t seq,
                               const std::vector<uint8_t>& payload,
                               const std::vector<HANDLE>& owned_handles) {
  if (!state || !state->channel || !state->channel->is_open()) {
    return false;
  }
  PlatformHandle stack[k_max_handles];
  const uint32_t n = static_cast<uint32_t>(
      std::min<size_t>(owned_handles.size(), k_max_handles));
  for (uint32_t i = 0; i < n; ++i) {
    stack[i] = PlatformHandle::borrow(owned_handles[i]);
  }
  const bool ok = state->channel->send(
      type, view_id, seq, payload.data(), static_cast<uint32_t>(payload.size()),
      n ? stack : nullptr, n);
  for (uint32_t i = 0; i < n; ++i) {
    if (owned_handles[i] && owned_handles[i] != INVALID_HANDLE_VALUE) {
      CloseHandle(owned_handles[i]);
    }
  }
  return ok;
}

bool ChannelProxy::send(uint16_t type,
                        uint32_t view_id,
                        uint32_t seq,
                        std::vector<uint8_t> payload,
                        const PlatformHandle* handles,
                        uint32_t handle_count) {
  if (!send_state_ || !send_state_->channel ||
      !send_state_->channel->is_open()) {
    return false;
  }
  if (handle_count > k_max_handles) {
    return false;
  }

  std::vector<HANDLE> owned;
  owned.reserve(handle_count);
  for (uint32_t i = 0; i < handle_count; ++i) {
    if (!handles || !handles[i].is_valid()) {
      continue;
    }
    HANDLE dup = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), handles[i].native,
                         GetCurrentProcess(), &dup, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      for (HANDLE h : owned) {
        CloseHandle(h);
      }
      return false;
    }
    owned.push_back(dup);
  }

  if (ScopedIpcSupport::is_active()) {
    auto state = send_state_;
    ScopedIpcSupport::post(
        [state, type, view_id, seq, payload = std::move(payload),
         owned = std::move(owned)]() mutable {
          (void)write_frame(state, type, view_id, seq, payload, owned);
          owned.clear();
        });
    return true;
  }

  const bool ok = write_frame(send_state_, type, view_id, seq, payload, owned);
  owned.clear();
  return ok;
}

bool ChannelProxy::send(uint16_t type,
                        uint32_t view_id,
                        const void* payload,
                        uint32_t payload_bytes) {
  std::vector<uint8_t> bytes;
  if (payload && payload_bytes) {
    const auto* p = static_cast<const uint8_t*>(payload);
    bytes.assign(p, p + payload_bytes);
  }
  return send(type, view_id, 0, std::move(bytes));
}

bool ChannelProxy::send_empty(uint16_t type, uint32_t view_id) {
  return send(type, view_id, 0, {});
}

}  // namespace ipc
}  // namespace common
