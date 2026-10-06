// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/link/bind_host.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/horizon/bind/bind_horizon.h"
#include "app/views/il.runtime/backend/plugin/bind_plugin.h"
#include "app/views/il.runtime/backend/document/document_bind.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/plugin/paths.h"
#include "app/views/il.runtime/backend/view/view_bind.h"

namespace app {

void bind_host(Browser& browser,
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

}  // namespace app
