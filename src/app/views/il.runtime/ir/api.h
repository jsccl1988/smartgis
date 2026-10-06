// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_API_H_
#define IL_RUNTIME_IR_API_H_

// Language-neutral calls over CapabilityHost. A script backend decodes its
// own syntax (.il CallStmt, or a future Python call), then enters here.
// This header does not parse a script, does not include Browser, and does
// not order suite steps.

#include "app/views/il.runtime/ir/document.h"
#include "app/views/il.runtime/ir/expect.h"
#include "app/views/il.runtime/ir/gesture.h"
#include "app/views/il.runtime/ir/horizon.h"
#include "app/views/il.runtime/ir/plugin.h"

#endif  // IL_RUNTIME_IR_API_H_
