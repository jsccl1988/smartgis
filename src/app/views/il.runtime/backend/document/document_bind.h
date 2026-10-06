// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_
#define IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_

#include "content/browser/capability/host.h"

namespace app {

class Browser;

namespace detail {

void bind_export(Browser& browser, content::CapabilityHost* out);

// Document slots, including export_bmp.
void register_document(Browser& browser, content::CapabilityHost* out);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_
