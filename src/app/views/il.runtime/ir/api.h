// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_API_H_
#define IL_RUNTIME_IR_API_H_

// Language-neutral calls over CapabilityHost. A script backend decodes its
// own syntax (.il CallStmt, or a future Python call), then enters here.
// This header does not parse a script, does not include Browser, and does
// not order suite steps.
//
// Four objects, matching content::CapabilityHost:
//   document.h  view.h  plugin.h  horizon.h
// fail.h is the shared exit-code helper.

#include "app/views/il.runtime/ir/document.h"
#include "app/views/il.runtime/ir/horizon.h"
#include "app/views/il.runtime/ir/plugin.h"
#include "app/views/il.runtime/ir/view.h"

#endif  // IL_RUNTIME_IR_API_H_
