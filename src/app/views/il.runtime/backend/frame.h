// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FRAME_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FRAME_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// Lower an export frame token onto the document camera.
// Tokens: china_product, unit_square, document_extent, or a plugin frame name.
bool resolve_export_frame(Browser& browser, const std::string& frame);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_FRAME_H_
