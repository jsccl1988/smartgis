// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/bind_host.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/bind_horizon.h"
#include "app/views/il.runtime/backend/bind_plugin.h"

namespace app {

void bind_host(Browser& browser,
               content::CapabilityHost* out,
               const wchar_t* mark_leaf) {
  if (!out) {
    return;
  }
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : detail::kUiShowcaseMarkLeaf;
  detail::bind_export(browser, out);
  detail::bind_plugin(browser, out);
  detail::bind_horizon(browser, out, leaf);
  detail::bind_expect(browser, out, leaf);
  out->fail_rc = 0;
}

}  // namespace app
