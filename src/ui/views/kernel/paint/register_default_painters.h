// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_PAINT_REGISTER_DEFAULT_PAINTERS_H_
#define UI_VIEWS_KERNEL_PAINT_REGISTER_DEFAULT_PAINTERS_H_

#include "ui/ui_export.h"

namespace ui {
namespace views {

// Idempotent: installs RoleForwardPainter for every toolkit paint_role.
UI_EXPORT void register_default_painters();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_PAINT_REGISTER_DEFAULT_PAINTERS_H_
