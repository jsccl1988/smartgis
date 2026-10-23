// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_
#define IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

namespace content {


namespace detail {

void bind_export(content::Shell& browser, content::CapabilityHost* out);

// Document slots, including export_bmp.
void register_document(content::Shell& browser, content::CapabilityHost* out);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_RUNTIME_DOCUMENT_BIND_H_
