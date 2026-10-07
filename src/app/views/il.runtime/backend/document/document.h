// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_

#include <string>

namespace content {
struct InputEvent;
}  // namespace content

namespace plugin {
class HarnessShell;
}

namespace app {

class Browser;

namespace detail {

bool dispatch_edit_input(Browser& browser, const content::InputEvent& event);
bool apply_style_file(Browser& browser, const std::string& path_utf8);
bool open_document(Browser& browser, const std::string& path_utf8);
bool open_document(plugin::HarnessShell& host, const std::string& path_utf8);
bool clear_map_document(Browser& browser);
bool fit_map_document(Browser& browser);
bool invalidate_map2d_frame(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_DOCUMENT_H_
