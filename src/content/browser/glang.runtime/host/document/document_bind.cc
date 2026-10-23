// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/document/document_bind.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

#include <functional>
#include <memory>
#include <string>

#include "content/browser/glang.runtime/host/document/document.h"
#include "content/browser/glang.runtime/host/bind/slots.h"

namespace content {
namespace detail {

void register_document(content::Shell& browser, content::CapabilityHost* out) {
  content::Shell* b = &browser;
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
               base::tag_resolver<slot_feature_count> =
                   [b]() { return feature_count(*b); },
           });
  // Second pack: layer / write verbs. Kept separate so MSVC tagged_tuple
  // reflection stays within a safe field count for Op assignment.
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_create_layer> =
                   [b](const std::string& name, const std::string& geometry) {
                     return create_layer(*b, name, geometry);
                   },
               base::tag_resolver<slot_remove_layer> =
                   [b](const std::string& id) { return remove_layer(*b, id); },
               base::tag_resolver<slot_set_layer_visible> =
                   [b](const std::string& id, bool visible) {
                     return set_layer_visible(*b, id, visible);
                   },
               base::tag_resolver<slot_select_layer> =
                   [b](const std::string& id) { return select_layer(*b, id); },
               base::tag_resolver<slot_write_document> =
                   [b](const std::string& path_utf8) {
                     return write_document(*b, path_utf8);
                   },
               base::tag_resolver<slot_clear_selection> =
                   [b]() { return clear_selection(*b); },
               base::tag_resolver<slot_layer_count> =
                   [b]() { return layer_count(*b); },
           });
  // String-returning Ops: assign with explicit Slot ctor (same as
  // host_member::assign) so MSVC does not leave a hollow callable.
  if (out) {
    using LayersSlot = decltype(out->document.layers_json);
    using ExtentSlot = decltype(out->document.extent_json);
    using HasStyleSlot = decltype(out->document.has_style_document);
    using ClearStyleSlot = decltype(out->document.clear_style);
    // Capture Shell* by value into a free-function-style thunk via
    // shared_ptr so Op::bind stores a stable callable (MSVC hollow-lambda
    // workaround for string-returning slots).
    auto layers_fn = std::make_shared<std::function<std::string()>>(
        [b]() -> std::string { return layers_json(*b); });
    out->document.layers_json =
        LayersSlot([layers_fn]() { return (*layers_fn)(); });
    out->document.extent_json =
        ExtentSlot([b]() { return extent_json(*b); });
    out->document.has_style_document =
        HasStyleSlot([b]() { return has_style_document(*b); });
    out->document.clear_style = ClearStyleSlot([b]() { clear_style(*b); });
  }
  bind_export(browser, out);
}

}  // namespace detail
}  // namespace content
