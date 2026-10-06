// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DEBUG_CONSOLE_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DEBUG_CONSOLE_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// Bind DebugAgentHost from Browser chrome and start the agent. 50 on start fail.
int wire_debug_agent(Browser& browser);

// Run one DebugAgent line. |contains| / |equals| / |reject| are optional
// substring / exact / forbidden checks. |fail_rc| is returned on mismatch.
int debug_exec(Browser& browser,
               const std::string& line,
               const std::string& contains,
               const std::string& equals,
               const std::string& reject,
               int fail_rc);

// Time view.pan drags + one wheel; write console_bench.json. 40–42 / 55.
int console_pan_bench(Browser& browser, const wchar_t* mark_leaf);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DEBUG_CONSOLE_H_
