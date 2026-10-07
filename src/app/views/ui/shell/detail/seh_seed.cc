// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/shell/detail/seh_seed.h"

#include "app/views/browser/browser.h"
#include "content/browser/session/browser_session.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

bool seh_seed_default(Browser* browser, bool allow_china) {
  if (!browser) {
    return false;
  }
  __try {
    browser->session().seed_default_document(allow_china);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool seh_fit_and_push_extent(Browser* browser) {
  if (!browser) {
    return false;
  }
  __try {
    browser->fit_map_extent();
    browser->push_shared_extent();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace detail
}  // namespace app
