// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

#include <string>

namespace content {
struct InputEvent;
}  // namespace content

namespace plugin {
class HarnessShell;
}

namespace content {


namespace detail {

bool dispatch_edit_input(content::ShellBridge& browser, const content::InputEvent& event);
bool apply_style_file(content::ShellBridge& browser, const std::string& path_utf8);
bool open_document(content::ShellBridge& browser, const std::string& path_utf8);
bool open_document(plugin::HarnessShell& host, const std::string& path_utf8);
bool clear_map_document(content::ShellBridge& browser);
bool fit_map_document(content::ShellBridge& browser);
bool invalidate_map2d_frame(content::ShellBridge& browser);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
