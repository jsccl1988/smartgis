// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_

#include "content/browser/glang.runtime/host/bind/shell.h"

#include <string>

namespace content {
struct InputEvent;
}  // namespace content

namespace plugin {
class HarnessShell;
}

namespace content {


namespace detail {

bool dispatch_edit_input(content::Shell& browser, const content::InputEvent& event);
bool apply_style_file(content::Shell& browser, const std::string& path_utf8);
bool open_document(content::Shell& browser, const std::string& path_utf8);
bool open_document(plugin::HarnessShell& host, const std::string& path_utf8);
bool clear_map_document(content::Shell& browser);
bool fit_map_document(content::Shell& browser);
bool invalidate_map2d_frame(content::Shell& browser);
int feature_count(content::Shell& browser);
bool create_layer(content::Shell& browser,
                  const std::string& name,
                  const std::string& geometry);
bool remove_layer(content::Shell& browser, const std::string& id);
bool set_layer_visible(content::Shell& browser,
                       const std::string& id,
                       bool visible);
bool select_layer(content::Shell& browser, const std::string& id);
bool write_document(content::Shell& browser, const std::string& path_utf8);
bool clear_selection(content::Shell& browser);
int layer_count(content::Shell& browser);
std::string layers_json(content::Shell& browser);
std::string extent_json(content::Shell& browser);
bool has_style_document(content::Shell& browser);
void clear_style(content::Shell& browser);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
