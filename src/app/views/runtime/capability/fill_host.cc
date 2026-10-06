// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/fill_host.h"

#include "app/views/browser/browser.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/runtime/capability/export_frame.h"
#include "app/views/runtime/capability/host_paths.h"
#include "app/views/runtime/capability/plugin_bind.h"
#include "app/views/runtime/capability/session_bind.h"
#include "app/views/runtime/capability/shell_bind.h"

namespace app {

void fill_host(Browser& browser,
               content::CapabilityHost* out,
               const wchar_t* mark_leaf) {
  if (!out) {
    return;
  }
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : detail::kUiShowcaseMarkLeaf;
  detail::bind_shell(browser, out, leaf);
  detail::bind_session(browser, out, leaf);
  detail::bind_paths(out);
  detail::bind_export(browser, out);
  detail::bind_plugin(browser, out);
}

}  // namespace app
