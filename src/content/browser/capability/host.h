// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HOST_H_
#define CONTENT_BROWSER_CAPABILITY_HOST_H_

#include "content/browser/capability/document.h"
#include "content/browser/capability/horizon.h"
#include "content/browser/capability/plugin.h"
#include "content/browser/capability/view.h"

namespace content {

// Callback bag for shared harness / DebugAgent scenario verbs.
// Four orthogonal objects. Each lane only fills its own member.
//   document — GisDocument + opened GisScene store verbs
//   view     — any ViewKind: present, edit input, tools, load facts
//   plugin   — PluginHost commands, processing, playback, reports
//   horizon  — shell tabs, HWND inject, marks, UI gates
// Filled by the shell (`app/views/il.runtime`); content must not depend on
// `app::Browser`. Fact reads notify IL-installed callbacks; verbs stay
// actions. Pass/fail lives in the IR layer.
struct CapabilityHost {
  DocumentCapability document;
  ViewCapability view;
  PluginCapability plugin;
  HorizonCapability horizon;

  // Non-zero when a gate/verb wants a specific process exit (IL maps false).
  int fail_rc = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HOST_H_
