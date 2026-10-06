// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_FAIL_H_
#define IL_RUNTIME_IR_FAIL_H_

#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Non-zero gate codes fail the step and stick on CapabilityHost::fail_rc.
// Shared by every object. Not a fifth capability.
inline bool apply_rc(content::CapabilityHost& host, int rc) {
  if (rc != 0) {
    host.fail_rc = rc;
    return false;
  }
  return true;
}

inline bool note_fail(content::CapabilityHost& host, bool ok, int fail_rc) {
  if (!ok && fail_rc != 0) {
    host.fail_rc = fail_rc;
  }
  return ok;
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_FAIL_H_
