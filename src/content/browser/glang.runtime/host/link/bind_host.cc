// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/link/bind_host.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include "content/browser/glang.runtime/host/horizon/bind/bind_horizon.h"
#include "content/browser/glang.runtime/host/plugin/bind_plugin.h"
#include "content/browser/glang.runtime/host/document/document_bind.h"
#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/plugin/paths.h"
#include "content/browser/glang.runtime/host/view/view_bind.h"

namespace content {

void bind_host(content::ShellBridge& browser,
               content::CapabilityHost* out,
               const wchar_t* mark_leaf) {
  if (!out) {
    return;
  }
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : detail::kUiMarkLeaf;
  detail::register_document(browser, out);
  detail::register_view(browser, out, leaf);
  detail::bind_plugin(browser, out);
  detail::bind_paths(out);
  detail::register_horizon(browser, out, leaf);
  out->fail_rc = 0;
}

}  // namespace content
