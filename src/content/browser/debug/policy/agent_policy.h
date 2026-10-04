// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_POLICY_H_
#define CONTENT_BROWSER_DEBUG_AGENT_POLICY_H_

#include <string>

namespace content {
namespace detail {

// Gate for dangerous DebugAgent operations (py / sdbd write / ui.click /
// capture). Loopback transport only; policy is in-process.
class AgentPolicy {
 public:
  AgentPolicy();

  // True when SG_DEBUG_ALLOW=1, debug build, or session confirmed.
  bool is_dangerous_allowed() const;

  // `:confirm` / rpc.confirm — session-scoped allow until process exit.
  void confirm_session();
  bool session_confirmed() const { return session_confirmed_; }

  // Classify console line / RPC method as needing the gate.
  static bool is_dangerous_console_line(const std::string& line);
  static bool is_dangerous_method(const std::string& method);

  // Emit an audit line to LogSink (NOTICE).
  static void audit(const std::string& action, const std::string& detail);

  // Params JSON may include "confirm":true to satisfy one RPC call when
  // session is already confirmed or env allows; otherwise returns false.
  bool allow_rpc(const std::string& method,
                 const std::string& params_json) const;

 private:
  bool env_allow_ = false;
  bool debug_build_allow_ = false;
  bool session_confirmed_ = false;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_POLICY_H_
