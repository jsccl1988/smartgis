// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_SELF_TEST_H_
#define APP_CEF_SELF_TEST_H_

namespace app {
namespace cef {

class LayoutHost;
class ChromeBridge;
class CefMapSlot;

// Runs Views-aligned --self-test semantics. Returns process exit code.
int run_self_test(LayoutHost& layout,
                  ChromeBridge& bridge,
                  CefMapSlot slots[3],
                  bool web_load_ok);

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_SELF_TEST_H_
