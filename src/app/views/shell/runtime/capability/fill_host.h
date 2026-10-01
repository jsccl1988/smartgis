// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_CAPABILITY_FILL_HOST_H_
#define APP_VIEWS_SHELL_RUNTIME_CAPABILITY_FILL_HOST_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

// Fills |out| from |browser|. |mark_leaf| is the exe-sidecar mark file name
// (e.g. L"ui-showcase-mark.txt"). No-op if |out| is null.
void fill_host(Browser& browser,
               content::CapabilityHost* out,
               const wchar_t* mark_leaf);

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_CAPABILITY_FILL_HOST_H_
