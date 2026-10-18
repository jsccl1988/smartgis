// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_API_H_
#define IL_RUNTIME_IR_API_H_

// Language-neutral calls over CapabilityHost. Python (smartgis.script)
// decodes arguments, then enters here.
// This header does not parse a script, does not include Browser, and does
// not order suite steps.
//
// Lanes match content::CapabilityHost:
//   document.h  view.h  plugin.h  horizon.h
// gis.h is the GdalGis backend. The host lane is content::GisCapability.
// fail.h is the shared exit-code helper.

#include "content/browser/glang.runtime/language/ir/document.h"
#include "content/browser/glang.runtime/language/ir/gis.h"
#include "content/browser/glang.runtime/language/ir/horizon.h"
#include "content/browser/glang.runtime/language/ir/plugin.h"
#include "content/browser/glang.runtime/language/ir/view.h"

#endif  // IL_RUNTIME_IR_API_H_
