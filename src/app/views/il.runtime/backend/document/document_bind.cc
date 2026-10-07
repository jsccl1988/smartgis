// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/document/document_bind.h"

#include <string>

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/document/document.h"
#include "app/views/il.runtime/bind/slots.h"

namespace app {
namespace detail {

void register_document(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
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
}  // namespace app
