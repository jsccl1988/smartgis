// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/portal.h"

#include "base/ipc/codec.h"
#include "base/ipc/invitation.h"

#include <chrono>
#include <map>

namespace base {
namespace ipc {

struct Portal::State {
  uint32_t id = 0;
  uint32_t peer_id = 0;
  bool remote = false;
  bool closed = false;
  Node* node = nullptr;
  Channel* link = nullptr;
  Channel dedicated;
  std::shared_ptr<State> local_peer;
  std::mutex mu;
  std::condition_variable cv;
  std::deque<PortalInboxMsg> inbox;
};

Portal::Portal(Portal&& other) noexcept : state_(std::move(other.state_)) {}

Portal& Portal::operator=(Portal&& other) noexcept {
  if (this != &other) {
    reset();
    state_ = std::move(other.state_);
  }
  return *this;
}

Portal::~Portal() {
  reset();
}

void Portal::reset() {
  state_.reset();
}

bool Portal::is_valid() const {
  return state_ && !state_->closed;
}

uint32_t Portal::id() const {
  return state_ ? state_->id : 0;
}

bool Portal::send(const void* data,
                  uint32_t bytes,
                  const PlatformHandle* handles,
                  uint32_t handle_count) {
  if (!is_valid() || (bytes > 0 && !data)) {
    return false;
  }
  PortalInboxMsg msg;
  if (bytes > 0) {
    const auto* p = static_cast<const uint8_t*>(data);
    msg.bytes.assign(p, p + bytes);
  }
  for (uint32_t i = 0; i < handle_count; ++i) {
    PlatformHandle dup;
    if (!wrap_into(handles[i].native, GetCurrentProcess(), &dup)) {
      return false;
    }
    msg.handles.push_back(std::move(dup));
  }
  Channel* wire = state_->dedicated.is_open() ? &state_->dedicated : nullptr;
  if (!wire && state_->remote) {
    wire = state_->link;
  }
  if (wire) {
    return wire->send(k_msg_portal_bytes, state_->peer_id, 0,
                      msg.bytes.data(),
                      static_cast<uint32_t>(msg.bytes.size()),
                      msg.handles.empty() ? nullptr : &msg.handles[0],
                      static_cast<uint32_t>(msg.handles.size()));
  }
  auto peer = state_->local_peer;
  if (!peer) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(peer->mu);
    peer->inbox.push_back(std::move(msg));
  }
  peer->cv.notify_one();
  return true;
}

bool Portal::recv(std::vector<uint8_t>* bytes,
                  std::vector<PlatformHandle>* handles,
                  uint32_t timeout_ms) {
  if (!is_valid() || !bytes) {
    return false;
  }
  if (state_->dedicated.is_open()) {
    Frame frame;
    std::vector<uint8_t> payload;
    std::vector<PlatformHandle> incoming;
    if (!state_->dedicated.recv(&frame, &payload, &incoming, timeout_ms)) {
      return false;
    }
    *bytes = std::move(payload);
    if (handles) {
      *handles = std::move(incoming);
    }
    return true;
  }
  std::unique_lock<std::mutex> lock(state_->mu);
  if (!state_->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
        return !state_->inbox.empty() || state_->closed;
      })) {
    return false;
  }
  if (state_->inbox.empty()) {
    return false;
  }
  PortalInboxMsg msg = std::move(state_->inbox.front());
  state_->inbox.pop_front();
  lock.unlock();
  *bytes = std::move(msg.bytes);
  if (handles) {
    *handles = std::move(msg.handles);
  }
  return true;
}

bool create_local_portal_pair(Portal* a, Portal* b) {
  if (!a || !b) {
    return false;
  }
  auto sa = std::make_shared<Portal::State>();
  auto sb = std::make_shared<Portal::State>();
  sa->id = 1;
  sb->id = 2;
  sa->peer_id = 2;
  sb->peer_id = 1;
  sa->local_peer = sb;
  sb->local_peer = sa;
  a->state_ = sa;
  b->state_ = sb;
  return true;
}

Node::Node() = default;

Node::~Node() {
  receiver_.reset();
}

bool Node::attach_peer(Channel* channel) {
  if (!channel || !channel->is_open()) {
    return false;
  }
  link_ = channel;
  receiver_ = std::make_unique<Receiver>(channel, this);
  return true;
}

bool Node::create_portal_pair(Portal* a, Portal* b) {
  if (!create_local_portal_pair(a, b)) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  a->state_->id = next_id_++;
  b->state_->id = next_id_++;
  a->state_->peer_id = b->state_->id;
  b->state_->peer_id = a->state_->id;
  a->state_->node = this;
  b->state_->node = this;
  a->state_->link = link_;
  b->state_->link = link_;
  table_[a->state_->id] = a->state_;
  table_[b->state_->id] = b->state_;
  return true;
}

bool Node::offer_portal(Portal* moving) {
  if (!moving || !moving->is_valid() || !link_) {
    return false;
  }
  auto st = moving->state_;
  auto peer = st->local_peer;
  if (!peer) {
    return false;
  }
  PlatformChannel local;
  PlatformChannel remote;
  if (!PlatformChannel::create_pair(&local, &remote)) {
    return false;
  }
  if (!peer->dedicated.adopt(local.release())) {
    return false;
  }
  PortalOpenBody body;
  body.keep_id = peer->id;
  body.move_id = st->id;
  PlatformHandle attached = PlatformHandle::borrow(remote.get());
  if (!link_->send_msg(k_msg_portal_open, st->id, body, &attached, 1)) {
    return false;
  }
  remote.close();
  peer->local_peer.reset();
  peer->remote = true;
  peer->peer_id = st->id;
  peer->link = &peer->dedicated;
  moving->reset();
  return true;
}

bool Node::take_portal(Portal* out, uint32_t timeout_ms) {
  if (!out) {
    return false;
  }
  std::unique_lock<std::mutex> lock(mu_);
  if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [&] { return !incoming_.empty(); })) {
    return false;
  }
  const uint32_t id = incoming_.front();
  incoming_.pop_front();
  auto it = table_.find(id);
  if (it == table_.end()) {
    return false;
  }
  out->state_ = it->second;
  return true;
}

void Node::deliver(uint32_t dest_id,
                   std::vector<uint8_t> bytes,
                   std::vector<PlatformHandle> handles) {
  std::shared_ptr<Portal::State> st;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = table_.find(dest_id);
    if (it == table_.end()) {
      return;
    }
    st = it->second;
  }
  PortalInboxMsg msg;
  msg.bytes = std::move(bytes);
  msg.handles = std::move(handles);
  {
    std::lock_guard<std::mutex> lock(st->mu);
    st->inbox.push_back(std::move(msg));
  }
  st->cv.notify_one();
}

void Node::on_message(const Frame& frame,
                      std::vector<uint8_t> payload,
                      std::vector<PlatformHandle> handles) {
  if (frame.type == k_msg_portal_open) {
    PortalOpenBody body;
    if (!decode(payload.data(), payload.size(), &body)) {
      return;
    }
    if (handles.empty() || !handles[0].is_valid()) {
      return;
    }
    auto st = std::make_shared<Portal::State>();
    st->id = body.move_id;
    st->peer_id = body.keep_id;
    st->remote = true;
    st->node = this;
    if (!st->dedicated.adopt(handles[0].release())) {
      return;
    }
    st->link = &st->dedicated;
    {
      std::lock_guard<std::mutex> lock(mu_);
      table_[st->id] = st;
      incoming_.push_back(st->id);
    }
    cv_.notify_one();
    return;
  }
  if (frame.type == k_msg_portal_bytes) {
    deliver(frame.view_id, std::move(payload), std::move(handles));
  }
}

}  // namespace ipc
}  // namespace base
