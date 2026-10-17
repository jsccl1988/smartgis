// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_LOG_AGENT_FEED_H_
#define BASE_LOG_AGENT_FEED_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "base/core/export.h"

namespace base {

// One DebugAgent command or execution line for the Diagnostic Tools Agent tab.
enum class AgentFeedKind {
  kCommand,
  kResult,
  kRpc,
  kDeny,
};

struct AgentFeedEntry {
  AgentFeedKind kind = AgentFeedKind::kCommand;
  std::string timestamp;
  std::string text;
  // Empty means the local Diagnostic Tools console. Remote connections use a
  // stable label such as "agent-2" or a client-supplied session name.
  std::string session;
};

// Process-wide ring in base.dll so the Views shell and DebugAgent share one feed.
// append / snapshot / subscribe are lock-free. Payload nodes are reclaimed with
// hazard pointers so a reader can copy a record while a writer reuses the slot.
class BASE_EXPORT AgentFeed {
 public:
  using Subscriber = std::function<void(const AgentFeedEntry&)>;

  AgentFeed();
  ~AgentFeed();

  AgentFeed(const AgentFeed&) = delete;
  AgentFeed& operator=(const AgentFeed&) = delete;

  void append(AgentFeedKind kind, std::string text);
  void append(AgentFeedKind kind, std::string text, std::string session);
  void clear();
  std::vector<AgentFeedEntry> snapshot() const;
  std::size_t size() const;

  // Callback runs on the appending thread and may run concurrently with another
  // append. Do not call append or clear from the subscriber.
  std::uint64_t subscribe(Subscriber cb);
  void unsubscribe(std::uint64_t id);

 private:
  struct Impl;
  Impl* impl_;
};

BASE_EXPORT AgentFeed& agent_feed();

}  // namespace base

#endif  // BASE_LOG_AGENT_FEED_H_
