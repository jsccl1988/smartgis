// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "common/ipc/broker/broker.h"

#include "common/ipc/codec/codec.h"

#include <atomic>

namespace common {
namespace ipc {
namespace {

std::atomic<uint32_t> g_broker_req{1};

}  // namespace

bool BrokerHost::try_handle(const Frame& frame,
                            const std::vector<uint8_t>& payload,
                            std::vector<PlatformHandle>* handles) {
  (void)handles;
  if (!channel_ || !channel_->is_open()) {
    return false;
  }
  if (frame.type != k_msg_broker_create_section) {
    return false;
  }
  BrokerCreateSectionBody req;
  if (!decode(payload.data(), payload.size(), &req) || req.byte_count == 0 ||
      req.byte_count > k_max_payload_bytes) {
    BrokerReplyBody fail;
    fail.request_id = req.request_id;
    fail.ok = 0;
    channel_->send_msg(k_msg_broker_reply, frame.view_id, fail);
    return true;
  }
  if (!client_ || client_ == INVALID_HANDLE_VALUE) {
    BrokerReplyBody fail;
    fail.request_id = req.request_id;
    fail.ok = 0;
    channel_->send_msg(k_msg_broker_reply, frame.view_id, fail);
    return true;
  }

  HANDLE section =
      CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                         req.byte_count, nullptr);
  BrokerReplyBody reply;
  reply.request_id = req.request_id;
  if (!section) {
    reply.ok = 0;
    channel_->send_msg(k_msg_broker_reply, frame.view_id, reply);
    return true;
  }

  PlatformHandle remote;
  if (!wrap_into(section, client_, &remote)) {
    CloseHandle(section);
    reply.ok = 0;
    channel_->send_msg(k_msg_broker_reply, frame.view_id, reply);
    return true;
  }
  CloseHandle(section);
  reply.ok = 1;
  const std::string body = encode(reply);
  const bool sent =
      channel_->send(k_msg_broker_reply, frame.view_id, 0, body.data(),
                     static_cast<uint32_t>(body.size()), &remote, 1);
  if (!sent) {
    remote.reset();
  }
  return true;
}

bool broker_create_section(Channel* channel,
                           uint32_t byte_count,
                           PlatformHandle* section_out,
                           uint32_t timeout_ms) {
  if (!channel || !channel->is_open() || !section_out || byte_count == 0) {
    return false;
  }
  section_out->reset();

  BrokerCreateSectionBody req;
  req.request_id = g_broker_req.fetch_add(1);
  req.byte_count = byte_count;
  if (!channel->send_msg(k_msg_broker_create_section, 0, req)) {
    return false;
  }

  const DWORD start = GetTickCount();
  while (GetTickCount() - start < timeout_ms) {
    Frame header{};
    std::vector<uint8_t> payload;
    std::vector<PlatformHandle> handles;
    const uint32_t left = timeout_ms - (GetTickCount() - start);
    if (!channel->recv(&header, &payload, &handles, left > 0 ? left : 1)) {
      continue;
    }
    if (header.type != k_msg_broker_reply) {
      continue;
    }
    BrokerReplyBody reply;
    if (!decode(payload.data(), payload.size(), &reply) ||
        reply.request_id != req.request_id) {
      continue;
    }
    if (!reply.ok || handles.empty() || !handles[0].is_valid()) {
      return false;
    }
    *section_out = std::move(handles[0]);
    return true;
  }
  return false;
}

}  // namespace ipc
}  // namespace common
