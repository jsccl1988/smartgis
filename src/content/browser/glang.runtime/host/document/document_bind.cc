// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/document/document_bind.h"
#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include <string>

#include "content/browser/glang.runtime/host/document/document.h"
#include "content/browser/glang.runtime/host/bind/slots.h"

namespace content {
namespace detail {

void register_document(content::ShellBridge& browser, content::CapabilityHost* out) {
  content::ShellBridge* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_open_document> =
                   [b](const std::string& path_utf8) {
                     return open_document(*b, path_utf8);
                   },
               base::tag_resolver<slot_doc_clear> =
                   [b]() { return clear_map_document(*b); },
               base::tag_resolver<slot_fit_extent> =
                   [b]() { return fit_map_document(*b); },
               base::tag_resolver<slot_apply_style_file> =
                   [b](const std::string& path_utf8) {
                     return apply_style_file(*b, path_utf8);
                   },
           });
  bind_export(browser, out);
}

}  // namespace detail
}  // namespace content
