// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/policy/agent_policy.h"

#include <cstdlib>

#include <rapidjson/document.h>

#include "base/core/log.h"

namespace content {
namespace detail {
namespace {

bool env_flag_on(const char* name) {
  const char* v = std::getenv(name);
  return v && v[0] == '1' && v[1] == '\0';
}

}  // namespace

AgentPolicy::AgentPolicy() {
  env_allow_ = env_flag_on("SG_DEBUG_ALLOW");
#if !defined(NDEBUG)
  debug_build_allow_ = true;
#else
  debug_build_allow_ = false;
#endif
}

bool AgentPolicy::is_dangerous_allowed() const {
  return env_allow_ || debug_build_allow_ || session_confirmed_;
}

void AgentPolicy::confirm_session() {
  session_confirmed_ = true;
  audit("confirm", "session dangerous ops allowed");
}

bool AgentPolicy::is_dangerous_console_line(const std::string& line) {
  if (line.rfind(":py ", 0) == 0 || line.rfind(":run ", 0) == 0) {
    return true;
  }
  if (line.rfind(":sdbd sql ", 0) == 0) {
    return true;
  }
  if (line.rfind(":ui click ", 0) == 0) {
    return true;
  }
  if (line == ":ui capture" || line.rfind(":ui capture ", 0) == 0) {
    return true;
  }
  // Bare Python one-liner (no leading ':') is also eval — gate it.
  if (!line.empty() && line[0] != ':') {
    return true;
  }
  return false;
}

bool AgentPolicy::is_dangerous_method(const std::string& method) {
  return method == "py.eval" || method == "py.run_file" ||
         method == "sdbd.query" || method == "ui.click" ||
         method == "ui.capture_shell";
}

void AgentPolicy::audit(const std::string& action, const std::string& detail) {
  LOGGING(LOG_NOTICE, "debug audit: %s %s", action.c_str(), detail.c_str());
}

bool AgentPolicy::allow_rpc(const std::string& method,
                            const std::string& params_json) const {
  if (!is_dangerous_method(method)) {
    return true;
  }
  if (is_dangerous_allowed()) {
    return true;
  }
  // One-shot confirm flag on the request (LLM / tools clients).
  rapidjson::Document doc;
  if (!doc.Parse(params_json.c_str()).HasParseError() && doc.IsObject()) {
    const auto it = doc.FindMember("confirm");
    if (it != doc.MemberEnd() && it->value.IsBool() && it->value.GetBool()) {
      return true;
    }
  }
  return false;
}

}  // namespace detail
}  // namespace content
