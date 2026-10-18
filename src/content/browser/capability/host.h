// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HOST_H_
#define CONTENT_BROWSER_CAPABILITY_HOST_H_

#include <concepts>
#include <type_traits>

#include "content/browser/capability/document.h"
#include "content/browser/capability/gis.h"
#include "content/browser/capability/horizon.h"
#include "content/browser/capability/plugin.h"
#include "content/browser/capability/view.h"

namespace content {

// Callback bag for shared harness / DebugAgent scenario verbs.
// Five lanes. Shell-filled lanes store concept-constrained ops.
//   document — GisDocument + opened GisScene store verbs
//   gis      — WKT geometry and feature relation (always the GIS backend)
//   view     — any ViewKind: present, edit input, tools, load facts
//   plugin   — PluginHost commands, processing, playback, reports
//   horizon  — shell tabs, HWND inject, marks, UI gates
// Filled by the shell registration in app/views. Content must not depend on
// `app::Browser`. GIS is not a shell slot. Fact reads notify IL-installed
// callbacks; verbs stay actions. Pass/fail lives in the IR layer.
struct CapabilityHost {
  DocumentCapability document;
  GisCapability gis;
  ViewCapability view;
  PluginCapability plugin;
  HorizonCapability horizon;

  // Non-zero when a gate/verb wants a specific process exit (IL maps false).
  int fail_rc = 0;
};

template <typename T>
concept capability_host = requires(T& t) {
  requires document_capability<std::remove_cvref_t<decltype(t.document)>>;
  requires gis_capability<std::remove_cvref_t<decltype(t.gis)>>;
  requires view_capability<std::remove_cvref_t<decltype(t.view)>>;
  requires plugin_capability<std::remove_cvref_t<decltype(t.plugin)>>;
  requires horizon_capability<std::remove_cvref_t<decltype(t.horizon)>>;
};

static_assert(capability_host<CapabilityHost>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HOST_H_
